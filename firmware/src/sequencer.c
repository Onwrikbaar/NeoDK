/*
 * sequencer.c -- the pulse train executor
 *
 *  NOTICE (do not remove):
 *      This file is part of project NeoDK (https://github.com/Onwrikbaar/NeoDK).
 *      See https://github.com/Onwrikbaar/NeoDK/blob/main/LICENSE.txt for full license details.
 *
 *  Created on: 27 Feb 2024
 *      Author: mark
 *   Copyright  2024 Neostim™
 */

#include <stdlib.h>

#include "bsp_dbg.h"
#include "bsp_mao.h"
#include "app_event.h"
#include "eventqueue.h"
#include "pattern_iter.h"

// This module implements:
#include "sequencer.h"

#define MAX_PULSE_WIDTH_MICROS           200

typedef void *(*StateFunc)(Sequencer *, AOEvent const *);

struct _Sequencer {
    EventQueue event_queue;                     // This MUST be the first member.
    uint8_t event_storage[400];
    StateFunc state;
    PatternIterator pi;
};

enum { EL_0, EL_A, EL_B, EL_C = 4, EL_AC = (EL_A | EL_C), EL_D = 8, EL_BD = (EL_B | EL_D) };

static char const elset_str[][5] = {
    "None", "A", "B", "AB", "C", "AC", "BC", "ABC", "D", "AD", "BD", "ABD", "CD", "ACD", "BCD", "ABCD"
};

static uint8_t const pattern_toggle[][2] =
{
    {EL_A, EL_B},
    {EL_C, EL_D},
    {EL_B, EL_A},
    {EL_D, EL_C},
};

static uint8_t const pattern_simple[][2] =
{
    {EL_AC, EL_BD},
    {EL_BD, EL_AC},
};

static uint8_t const pattern_circle[][2] =
{
    {EL_A,  EL_B},
    {EL_B,  EL_AC},
    {EL_C,  EL_B},
    {EL_BD, EL_C},
    {EL_C,  EL_D},
    {EL_D,  EL_AC},
    {EL_A,  EL_D},
    {EL_BD, EL_A},
    {EL_AC, EL_BD},

    {EL_B,  EL_A},
    {EL_AC, EL_B},
    {EL_B,  EL_C},
    {EL_C,  EL_BD},
    {EL_D,  EL_C},
    {EL_AC, EL_D},
    {EL_D,  EL_A},
    {EL_A,  EL_BD},
    {EL_BD, EL_AC},
};


static void printAdcValues(uint16_t const *v)
{
    uint32_t Iprim_mA = ((uint32_t)v[0] * 2063UL) / 1024;
    uint32_t Vcap_mV = ((uint32_t)v[1] * 52813UL) / 16384;
    uint32_t Vbat_mV = ((uint32_t)v[2] * 52813UL) / 16384;
    BSP_logf("Iprim=%u mA, Vcap=%u mV, Vbat=%u mV\n", Iprim_mA, Vcap_mV, Vbat_mV);
}

// Forward declaration.
static void *statePulsing(Sequencer *, AOEvent const *);


static void *stateIdle(Sequencer *me, AOEvent const *evt)
{
    switch (AOEvent_type(evt))
    {
        case ET_AO_ENTRY:
            BSP_logf("%s ENTRY\n", __func__);
            // TODO Choose a different pattern each time?
            // PatternIterator_init(&me->pi, pattern_toggle, M_DIM(pattern_toggle), 80, 400, 3);
            // PatternIterator_init(&me->pi, pattern_simple, M_DIM(pattern_simple), 50, 4, 7);
            PatternIterator_init(&me->pi, pattern_circle, M_DIM(pattern_circle), 25, 10, 5);
            break;
        case ET_AO_EXIT:
            BSP_logf("%s EXIT\n", __func__);
            break;
        case ET_ADC_DATA_AVAILABLE:
            printAdcValues((uint16_t const *)AOEvent_data(evt));
            break;
        case ET_SEQUENCER_PLAY_PAUSE:
            M_ASSERT(! PatternIterator_done(&me->pi));
            return &statePulsing;               // Transition.
        case ET_BURST_COMPLETED:
            BSP_logf("Pulse train last pulse done\n");
            break;
        case ET_BURST_EXPIRED:
            BSP_logf("Pulse train finished\n");
            break;
        default:
            BSP_logf("Sequencer_%s unexpected event: %u\n", __func__, AOEvent_type(evt));
    }
    return NULL;
}


static void *statePaused(Sequencer *me, AOEvent const *evt)
{
    switch (AOEvent_type(evt))
    {
        case ET_AO_ENTRY:
            BSP_logf("%s ENTRY\n", __func__);
            break;
        case ET_AO_EXIT:
            BSP_logf("%s EXIT\n", __func__);
            break;
        case ET_SEQUENCER_PLAY_PAUSE:
            BSP_logf("Resuming...\n");
            return &statePulsing;
        case ET_BURST_COMPLETED:
            // BSP_logf("Last pulse done\n");
            break;
        case ET_BURST_EXPIRED:
            if (PatternIterator_done(&me->pi)) {
                BSP_logf("Last pulse train finished\n");
                return &stateIdle;              // Transition.
            }
            BSP_logf("Pulse train finished - pausing\n");
            break;
        default:
            BSP_logf("Sequencer_%s unexpected event: %u\n", __func__, AOEvent_type(evt));
    }
    return NULL;
}


static bool scheduleNextPulseTrain(Sequencer *me)
{
    PulseTrain pt;
    if (PatternIterator_getNextPulseTrain(&me->pi, &pt)) {
        if (pt.pulse_width_micros > MAX_PULSE_WIDTH_MICROS) {
            pt.pulse_width_micros = MAX_PULSE_WIDTH_MICROS;
        }
        BSP_logf("Pulse width is %hu µs\n", pt.pulse_width_micros);
        return BSP_startPulseTrain(&pt);
    }

    return false;
}


static void *statePulsing(Sequencer *me, AOEvent const *evt)
{
    switch (AOEvent_type(evt))
    {
        case ET_AO_ENTRY:
            BSP_logf("%s ENTRY\n", __func__);
            scheduleNextPulseTrain(me);
            break;
        case ET_AO_EXIT:
            BSP_logf("%s EXIT\n", __func__);
            break;
        case ET_SEQUENCER_PLAY_PAUSE:
            return &statePaused;                // Transition.
        case ET_BURST_STARTED:
            // BSP_logf("Pulse train started\n");
            break;
        case ET_BURST_COMPLETED:
            // BSP_logf("Last pulse done\n");
            break;
        case ET_BURST_EXPIRED:
            if (! scheduleNextPulseTrain(me)) {
                BSP_logf("Last pulse train finished\n");
                return &stateIdle;              // Transition.
            }
            break;
        case ET_ADC_DATA_AVAILABLE:
            printAdcValues((uint16_t const *)AOEvent_data(evt));
            break;
        default:
            BSP_logf("Sequencer_%s unexpected event: %u\n", __func__, AOEvent_type(evt));
    }
    return NULL;
}

// Send one event to the state machine.
static void dispatchEvent(Sequencer *me, AOEvent const *evt)
{
    // BSP_logf("%s(%u)\n", __func__, evt);
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
    me->state = &stateIdle;
    BSP_registerPulseDelegate(&me->event_queue);
    return me;
}


void Sequencer_start(Sequencer *me)
{
    PatternIterator_checkPattern(pattern_toggle, M_DIM(pattern_toggle));
    PatternIterator_checkPattern(pattern_simple, M_DIM(pattern_simple));
    PatternIterator_checkPattern(pattern_circle, M_DIM(pattern_circle));

    PatternIterator_init(&me->pi, pattern_circle, 8/*M_DIM(pattern_circle)*/, 40, 1, 2);
    uint32_t total_nr_of_pulses = 0;
    PulseTrain pt;
    while (PatternIterator_getNextPulseTrain(&me->pi, &pt)) {
        BSP_logf(" %3hu * {%s, %s}\n", pt.nr_of_pulses, elset_str[pt.elcon[0]], elset_str[pt.elcon[1]]);
        total_nr_of_pulses += pt.nr_of_pulses;
    }
    BSP_logf("Total number of pulses: %u\n", total_nr_of_pulses);
    me->state(me, AOEvent_newEntryEvent());
}


bool Sequencer_handleEvent(Sequencer *me)
{
    return EventQueue_handleNextEvent(&me->event_queue, (EvtFunc)&dispatchEvent, me);
}


void Sequencer_stop(Sequencer *me)
{
    BSP_primaryVoltageEnable(false);
}


void Sequencer_delete(Sequencer *me)
{
    free(me);
}
