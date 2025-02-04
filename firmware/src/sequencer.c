/*
 * sequencer.c -- the pulse train executor
 *
 *  NOTICE (do not remove):
 *      This file is part of project NeoDK (https://github.com/Onwrikbaar/NeoDK).
 *      See https://github.com/Onwrikbaar/NeoDK/blob/main/LICENSE.txt for full license details.
 *
 *  Created on: 27 Feb 2024
 *      Author: mark
 *   Copyright  2024, 2025 Neostim™
 */

#include <stdlib.h>
#include <string.h>

#include "bsp_dbg.h"
#include "debug_cli.h"
#include "app_event.h"
#include "attributes.h"
#include "pattern_iter.h"
#include "ptd_queue.h"

// This module implements:
#include "sequencer.h"

#define DEFAULT_INTENSITY_PERCENT         16

typedef void *(*StateFunc)(Sequencer *, AOEvent const *);

struct _Sequencer {
    EventQueue event_queue;                     // This MUST be the first member.
    uint8_t event_storage[200];
    PtdQueue *ptd_queue;
    StateFunc state;
    PatternDescr const *pattern;
    PatternIterator pi;
    uint8_t intensity_percent;
    uint8_t play_state;
    uint8_t stream_busy;
    uint8_t stream_paused;
};


static void setPrimaryVoltage_mV(uint16_t new_voltage_mV)
{
    static uint16_t current_voltage_mV = 0;
    if (new_voltage_mV != current_voltage_mV) {
        BSP_logf("Setting Vcap to %hu mV\n", new_voltage_mV);
        BSP_setPrimaryVoltage_mV(new_voltage_mV);
    }
}


static void setIntensityPercentage(Sequencer *me, uint8_t perc)
{
    BSP_logf("Setting intensity to %hhu%%\n", perc);
    me->intensity_percent = perc;
    // TODO Ramp up to the previous intensity?
    setPrimaryVoltage_mV(perc * 80);
    Sequencer_notifyIntensity(me);
    PatternIterator_setPulseWidth(&me->pi, 50 + perc + perc / 2);
}


static void switchPattern(Sequencer *me, PatternDescr const *pd)
{
    CLI_logf("Switching to '%s'\n", Patterns_name(pd));
    me->pattern = pd;
    PatternIterator_init(&me->pi, pd);
    setIntensityPercentage(me, DEFAULT_INTENSITY_PERCENT);
    Sequencer_notifyPattern(me);
}


static void setPlayState(Sequencer *me, PlayState play_state)
{
    me->play_state = play_state;
    Sequencer_notifyPlayState(me);
}


static void handleAdcValues(uint16_t const *v, TransactionId trans_id)
{
    struct {
        uint16_t Vbat_mV, Vcap_mV, Iprim_mA;
    } vi = {
        .Vbat_mV = ((uint32_t)v[2] * 52813UL) / 16384,
        .Vcap_mV = ((uint32_t)v[1] * 52813UL) / 16384,
        .Iprim_mA = ((uint32_t)v[0] * 2063UL) /  1024,
    };
    CLI_logf("Vbat=%hu mV, Vcap=%hu mV, Iprim=%hu mA\n", vi.Vbat_mV, vi.Vcap_mV, vi.Iprim_mA);
    Attribute_changed(AI_VOLTAGES, trans_id, EE_BYTES_1LEN, (uint8_t const *)&vi, sizeof vi);
}


static void *stateNop(Sequencer *me, AOEvent const *evt)
{
    BSP_logf("Sequencer_%s unexpected event: %u\n", __func__, AOEvent_type(evt));
    return NULL;
}


static void queueDescriptor(Sequencer *me, PulseTrain const *pt, uint16_t sz)
{
    uint8_t err = PE_NONE;
    if (PtdQueue_addDescriptor(me->ptd_queue, pt, sz, &err)) {
        Sequencer_notifyPtQueue(me, NO_TRANS_ID);
    } else {
        BSP_logf("Pt discarded, err=%u:\n  ", err);
        PulseTrain_print(pt, sz);
    }
}

// Forward declarations.
static void *stateIdle(Sequencer *, AOEvent const *);
static void *statePulsing(Sequencer *, AOEvent const *);


static void *stateCanopy(Sequencer *me, AOEvent const *evt)
{
    switch (AOEvent_type(evt))
    {
        case ET_ADC_DATA_AVAILABLE:
            handleAdcValues((uint16_t const *)AOEvent_data(evt), 0);
            break;
        case ET_STOP:
            return &stateIdle;                  // Transition.
        case ET_SELECT_NEXT_PATTERN:
            switchPattern(me, Patterns_getNext(me->pattern));
            break;
        case ET_SELECT_PATTERN_BY_NAME: {
            PatternDescr const *pd = Patterns_findByName((char const *)AOEvent_data(evt), AOEvent_dataSize(evt));
            if (pd != NULL) { switchPattern(me, pd); break; }
            // me->pattern = NULL;
            return &stateIdle;                  // Transition.
        }
        case ET_QUEUE_PULSE_TRAIN:
            queueDescriptor(me, (PulseTrain const *)AOEvent_data(evt), AOEvent_dataSize(evt));
            break;
        case ET_SET_INTENSITY:
            setIntensityPercentage(me, *AOEvent_data(evt));
            break;
        case ET_BURST_COMPLETED:
            // BSP_logf("Last pulse done\n");
            break;
        case ET_UNKNOWN_COMMAND:
            BSP_logf("Unknown command\n");
            break;
        default:
            BSP_logf("Sequencer_%s unexpected event: %u\n", __func__, AOEvent_type(evt));
    }
    return NULL;
}


static bool scheduleFirstBurst(Sequencer *me)
{
    Burst burst;
    bool ok = PtdQueue_getNextBurst(me->ptd_queue, &burst);
    if (ok) {
        BSP_startSequencerClock(burst.start_time_µs - 40);
        ok = BSP_scheduleBurst(&burst);
    }
    return ok;
}


static bool scheduleNextBurst(Sequencer *me)
{
    Burst burst;
    bool ok = PtdQueue_getNextBurst(me->ptd_queue, &burst) && BSP_scheduleBurst(&burst);
    if (burst.flags & QF_QUEUE_CHANGED) Sequencer_notifyPtQueue(me, NO_TRANS_ID);
    return ok;
}


static void pauseStream(Sequencer *me)
{
    BSP_stopSequencerClock();
    setPlayState(me, PS_PAUSED);
    CLI_logf("Pausing stream\n");
}


static void resumeStream(Sequencer *me)
{
    setPlayState(me, PS_PLAYING);
    CLI_logf("Resuming stream\n");
    BSP_resumeSequencerClock();
}


static void *stateStreaming(Sequencer *me, AOEvent const *evt)
{
    static PatternDescr const stream_pd = { .name = "<stream>" };

    switch (AOEvent_type(evt))
    {
        case ET_AO_ENTRY:
            // me->pattern = &stream_pd;
            me->pi.pattern_descr = &stream_pd;
            BSP_logf("Sequencer_%s ENTRY\n", __func__);
            setPlayState(me, PS_PLAYING);
            me->stream_busy = scheduleFirstBurst(me);
            me->stream_paused = false;
            break;
        case ET_AO_EXIT:
            BSP_stopSequencerClock();
            PtdQueue_clear(me->ptd_queue);
            Sequencer_notifyPtQueue(me, NO_TRANS_ID);
            BSP_logf("Sequencer_%s EXIT\n", __func__);
            break;
        case ET_START_STREAM:
            // Superfluous, ignore.
            break;
        case ET_PLAY:
            if (me->stream_paused) resumeStream(me);
            break;
        case ET_PAUSE:
            if (!me->stream_paused) pauseStream(me);
            break;
        case ET_TOGGLE_PLAY_PAUSE:
            if (me->stream_paused) resumeStream(me);
            else pauseStream(me);
            break;
        case ET_STOP_STREAM:
            return &stateIdle;                  // Transition.
        case ET_BURST_STARTED:
            me->stream_busy = scheduleNextBurst(me);
            break;
        case ET_BURST_COMPLETED:
            // BSP_logf("Last pulse done\n");
            break;
        case ET_BURST_EXPIRED:
            if (me->stream_busy) break;
            return &stateIdle;                  // No more descriptors, leave this state.
        default:
            return stateCanopy(me, evt);        // Forward the event.
    }
    return NULL;
}


static void *stateIdle(Sequencer *me, AOEvent const *evt)
{
    switch (AOEvent_type(evt))
    {
        case ET_AO_ENTRY:
            BSP_logf("Sequencer_%s ENTRY\n", __func__);
            setPlayState(me, PS_IDLE);
            break;
        case ET_AO_EXIT:
            BSP_logf("Sequencer_%s EXIT\n", __func__);
            break;
        case ET_TOGGLE_PLAY_PAUSE:
        case ET_PLAY:
            if (PatternIterator_init(&me->pi, me->pattern)) {
                CLI_logf("Starting '%s'\n", PatternIterator_name(&me->pi));
                return &statePulsing;           // Transition.
            }
            break;
        case ET_START_STREAM:
            if (PtdQueue_isEmpty(me->ptd_queue)) break;
            return &stateStreaming;             // Transition.
        case ET_STOP_STREAM:
            // Superfluous, ignore.
            break;
        case ET_BURST_EXPIRED:
            CLI_logf("Finished '%s'\n", PatternIterator_name(&me->pi));
            break;
        default:
            return stateCanopy(me, evt);        // Forward the event.
    }
    return NULL;
}


static void *statePaused(Sequencer *me, AOEvent const *evt)
{
    switch (AOEvent_type(evt))
    {
        case ET_AO_ENTRY:
            BSP_logf("Sequencer_%s ENTRY\n", __func__);
            setPlayState(me, PS_PAUSED);
            break;
        case ET_AO_EXIT:
            BSP_logf("Sequencer_%s EXIT\n", __func__);
            break;
        case ET_SELECT_NEXT_PATTERN:
            switchPattern(me, Patterns_getNext(me->pattern));
            return &stateIdle;                  // Transition.
        case ET_SELECT_PATTERN_BY_NAME: {
            PatternDescr const *pd = Patterns_findByName((char const *)AOEvent_data(evt), AOEvent_dataSize(evt));
            if (pd != NULL) switchPattern(me, pd);
            // else me->pattern = NULL;
            return &stateIdle;                  // Transition.
        }
        case ET_TOGGLE_PLAY_PAUSE:
            CLI_logf("Resuming '%s'\n", PatternIterator_name(&me->pi));
            // Fall through.
        case ET_PLAY:
            return &statePulsing;               // Transition.
        case ET_BURST_EXPIRED:
            if (PatternIterator_done(&me->pi)) {
                CLI_logf("Finished '%s'\n", PatternIterator_name(&me->pi));
                return &stateIdle;              // Transition.
            }
            CLI_logf("Pausing '%s'\n", PatternIterator_name(&me->pi));
            break;
        default:
            return stateCanopy(me, evt);        // Forward the event.
    }
    return NULL;
}


static void *statePulsing(Sequencer *me, AOEvent const *evt)
{
    switch (AOEvent_type(evt))
    {
        case ET_AO_ENTRY:
            BSP_logf("Sequencer_%s ENTRY\n", __func__);
            setPlayState(me, PS_PLAYING);
            PatternIterator_scheduleNextBurst(&me->pi);
            break;
        case ET_AO_EXIT:
            BSP_logf("Sequencer_%s EXIT\n", __func__);
            break;
        case ET_TOGGLE_PLAY_PAUSE:
        case ET_PAUSE:
            return &statePaused;                // Transition.
        case ET_BURST_STARTED:
            // BSP_logf("Burst started\n");
            break;
        case ET_BURST_EXPIRED:
            if (! PatternIterator_scheduleNextBurst(&me->pi)) {
                CLI_logf("Finished '%s'\n", PatternIterator_name(&me->pi));
                return &stateIdle;              // Transition.
            }
            break;
        default:
            return stateCanopy(me, evt);        // Forward the event.
    }
    return NULL;
}

// Send one event to the state machine.
static void dispatchEvent(Sequencer *me, AOEvent const *evt)
{
    // BSP_logf("%s(%u)\n", __func__, AOEvent_type(evt));
    StateFunc new_state = me->state(me, evt);
    if (new_state != NULL) {                    // Transition.
        StateFunc sf = me->state(me, AOEvent_newExitEvent());
        // No transition allowed on ENTRY and EXIT events.
        M_ASSERT(sf == NULL);
        me->state = new_state;
        sf = me->state(me, AOEvent_newEntryEvent());
        M_ASSERT(sf == NULL);
    }
}

/*
 * Below are the functions implementing this module's interface.
 */

Sequencer *Sequencer_new()
{
    Sequencer *me = (Sequencer *)malloc(sizeof(Sequencer));
    EventQueue_init(&me->event_queue, me->event_storage, sizeof me->event_storage);
    me->ptd_queue = PtdQueue_new(20);
    return me;
}


Sequencer *Sequencer_init(Sequencer *me)
{
    me->state = &stateNop;
    char const default_pattern_name[] = "Toggle";
    me->pattern = Patterns_findByName(default_pattern_name, strlen(default_pattern_name));
    me->intensity_percent = 0;
    me->play_state = PS_UNKNOWN;
    BSP_registerPulseDelegate(&me->event_queue);
    return me;
}


void Sequencer_start(Sequencer *me)
{
    Patterns_checkAll();
    me->stream_busy = false;
    me->stream_paused = false;
    me->state = stateIdle;
    me->state(me, AOEvent_newEntryEvent());
    setIntensityPercentage(me, DEFAULT_INTENSITY_PERCENT);
    BSP_primaryVoltageEnable(true);
}


bool Sequencer_handleEvent(Sequencer *me)
{
    return EventQueue_handleNextEvent(&me->event_queue, (EvtFunc)&dispatchEvent, me);
}


uint8_t Sequencer_getIntensityPercentage(Sequencer const *me)
{
    return me->intensity_percent;
}


void Sequencer_notifyIntensity(Sequencer const *me)
{
    Attribute_changed(AI_INTENSITY_PERCENT, NO_TRANS_ID, EE_UNSIGNED_INT_1, &me->intensity_percent, sizeof me->intensity_percent);
}


void Sequencer_notifyPattern(Sequencer const *me)
{
    char const *name = Patterns_name(me->pattern);
    Attribute_changed(AI_CURRENT_PATTERN_NAME, NO_TRANS_ID, EE_UTF8_1LEN, (uint8_t const *)name, strlen(name));
}


void Sequencer_notifyPlayState(Sequencer const *me)
{
    Attribute_changed(AI_PLAY_PAUSE_STOP, NO_TRANS_ID, EE_UNSIGNED_INT_1, &me->play_state, sizeof me->play_state);
}


void Sequencer_notifyPtQueue(Sequencer const *me, TransactionId trans_id)
{
    uint16_t nqbf[2];                           // We have one queue per phase.
    PtdQueue_nrOfBytesFree(me->ptd_queue, nqbf);
    Attribute_changed(AI_PT_DESCRIPTOR_QUEUE, trans_id, EE_BYTES_1LEN, (uint8_t const *)nqbf, sizeof nqbf);
}


void Sequencer_stop(Sequencer *me)
{
    BSP_primaryVoltageEnable(false);
    me->state(me, AOEvent_newExitEvent());
    me->state = stateNop;
}


void Sequencer_delete(Sequencer *me)
{
    PtdQueue_delete(me->ptd_queue);
    free(me);
}
