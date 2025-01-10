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
#include "bsp_mao.h"
#include "debug_cli.h"
#include "app_event.h"
#include "attributes.h"
#include "pattern_iter.h"
#include "pulse_train.h"

// This module implements:
#include "sequencer.h"

#define DEFAULT_INTENSITY_PERCENT         15

typedef void *(*StateFunc)(Sequencer *, AOEvent const *);

struct _Sequencer {
    EventQueue event_queue;                     // This MUST be the first member.
    uint8_t event_storage[200];
    EventQueue ptd_queue;
    uint8_t ptd_storage[400];
    StateFunc state;
    PatternDescr const *pattern_descr;
    PatternIterator pi;
    uint8_t nr_of_patterns;
    uint8_t pattern_index;
    uint8_t intensity_percent;
    uint8_t play_state;
};

enum { EL_0, EL_A, EL_B, EL_C = 4, EL_AC = (EL_A | EL_C), EL_D = 8, EL_BD = (EL_B | EL_D) };

__attribute__((unused))
static char const elset_str[][5] = {
    "None", "A", "B", "AB", "C", "AC", "BC", "ABC", "D", "AD", "BD", "ABD", "CD", "ACD", "BCD", "ABCD"
};

static uint8_t const pattern_jackhammer[][2] =
{
    {EL_AC, EL_BD},
    {EL_BD, EL_AC},
};

static uint8_t const pattern_toggle[][2] =
{
    {EL_A, EL_B},
    {EL_C, EL_D},
    {EL_B, EL_A},
    {EL_D, EL_C},
};

static uint8_t const pattern_cross_toggle[][2] =
{
    {EL_A, EL_D},
    {EL_B, EL_C},
    {EL_C, EL_B},
    {EL_D, EL_A},
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

static uint8_t const pattern_itch[][2] =
{
    {EL_A, EL_B},
    {EL_B, EL_A},
    {EL_A, EL_B},
    {EL_B, EL_A},

    // {EL_AC, EL_BD},

    {EL_C, EL_D},
    {EL_D, EL_C},
    {EL_C, EL_D},
    {EL_D, EL_C},

    // {EL_BD, EL_AC},

    {EL_A, EL_B},
    {EL_C, EL_D},
    {EL_B, EL_A},
    {EL_D, EL_C},
};

static PatternDescr const pattern_descriptors[] =
{
    {
        .name = "Jackhammer",
        .pattern = pattern_jackhammer,
        .nr_of_elcons = M_DIM(pattern_jackhammer),
        .pace_µs = MAX_PULSE_PACE_µs,
        .nr_of_steps = 3,
        .nr_of_reps = 200,
    },
    {
        .name = "Toggle",
        .pattern = pattern_toggle,
        .nr_of_elcons = M_DIM(pattern_toggle),
        .pace_µs = 50000,
        .nr_of_steps = 5,
        .nr_of_reps = 300,
    },
    {
        .name = "CrossToggle",
        .pattern = pattern_cross_toggle,
        .nr_of_elcons = M_DIM(pattern_cross_toggle),
        .pace_µs = 20000,
        .nr_of_steps = 5,
        .nr_of_reps = 200,
    },
    {
        .name = "Circle",
        .pattern = pattern_circle,
        .nr_of_elcons = M_DIM(pattern_circle),
        .pace_µs = 30000,
        .nr_of_steps = 9,
        .nr_of_reps = 40,
    },
    {
        .name = "Scratch that itch",
        .pattern = pattern_itch,
        .nr_of_elcons = M_DIM(pattern_itch),
        .pace_µs = 8000,
        .nr_of_steps = 11,
        .nr_of_reps = 5000,
    },
};


static void checkAllPatterns(Sequencer *me)
{
    for (uint8_t i = 0; i < me->nr_of_patterns; i++) {
        PatternDescr const *pd = &me->pattern_descr[i];
        BSP_logf("Checking '%s'\n", pd->name);
        PatternIterator_checkPattern(pd->pattern, pd->nr_of_elcons);
    }
}


static void selectPatternByName(Sequencer *me, char const *name, EventSize len)
{
    for (uint8_t i = 0; i < me->nr_of_patterns; i++) {
        char const *pattern_name = me->pattern_descr[i].name;
        if (len == strlen(pattern_name) && memcmp(name, pattern_name, len) == 0) {
            // BSP_logf("%s '%s'\n", __func__, pattern_name);
            me->pattern_index = i;
            return;
        }
    }
}


static void setPulseWidth(Sequencer *me, uint8_t width_µs)
{
    BSP_logf("Setting pulse width to %hhu µs\n", width_µs);
    me->pi.pulse_width_micros = width_µs;
}


static void setIntensityPercentage(Sequencer *me, uint8_t perc)
{
    BSP_logf("Setting intensity to %hhu%%\n", perc);
    me->intensity_percent = perc;
    // TODO Ramp up to the previous intensity?
    Sequencer_notifyIntensity(me);
    BSP_setPrimaryVoltage_mV(perc * 80);
    setPulseWidth(me, 50 + perc + perc / 2);
}


static void switchPattern(Sequencer *me)
{
    PatternDescr const *pd = &me->pattern_descr[me->pattern_index];
    CLI_logf("Switching to '%s'\n", pd->name);
    PatternIterator_init(&me->pi, pd);
    setIntensityPercentage(me, DEFAULT_INTENSITY_PERCENT);
    Sequencer_notifyPattern(me);
}


static void setPlayState(Sequencer *me, PlayState play_state)
{
    me->play_state = play_state;
    Sequencer_notifyPlayState(me);
}


static void handleAdcValues(uint16_t const *v)
{
    struct {
        uint16_t Vbat_mV, Vcap_mV, Iprim_mA;
    } vi = {
        .Vbat_mV = ((uint32_t)v[2] * 52813UL) / 16384,
        .Vcap_mV = ((uint32_t)v[1] * 52813UL) / 16384,
        .Iprim_mA = ((uint32_t)v[0] * 2063UL) /  1024,
    };
    CLI_logf("Vbat=%hu mV, Vcap=%hu mV, Iprim=%hu mA\n", vi.Vbat_mV, vi.Vcap_mV, vi.Iprim_mA);
    Attribute_changed(AI_VOLTAGES, EE_BYTES_1LEN, (uint8_t const *)&vi, sizeof vi);
}


static void *stateNop(Sequencer *me, AOEvent const *evt)
{
    BSP_logf("Sequencer_%s unexpected event: %u\n", __func__, AOEvent_type(evt));
    return NULL;
}

// Forward declarations.
static void *stateIdle(Sequencer *, AOEvent const *);
static void *statePulsing(Sequencer *, AOEvent const *);


static void *stateCanopy(Sequencer *me, AOEvent const *evt)
{
    switch (AOEvent_type(evt))
    {
        case ET_ADC_DATA_AVAILABLE:
            handleAdcValues((uint16_t const *)AOEvent_data(evt));
            break;
        case ET_STOP:
            return &stateIdle;                  // Transition.
        case ET_SELECT_NEXT_PATTERN:
            if (++me->pattern_index == me->nr_of_patterns) me->pattern_index = 0;
            switchPattern(me);
            break;
        case ET_SELECT_PATTERN_BY_NAME:
            selectPatternByName(me, (char const *)AOEvent_data(evt), AOEvent_dataSize(evt));
            switchPattern(me);
            break;
        case ET_SET_INTENSITY:
            setIntensityPercentage(me, *(uint8_t const *)AOEvent_data(evt));
            break;
        case ET_BURST_COMPLETED:
            // BSP_logf("Last pulse done\n");
            break;
        default:
            BSP_logf("Sequencer_%s unexpected event: %u\n", __func__, AOEvent_type(evt));
    }
    return NULL;
}


static bool startBurst(Burst const *burst, Deltas const *deltas)
{
    if (Burst_isValid(burst)) {
        return BSP_startBurst(burst, deltas);
    }

    BSP_logf("Invalid burst\n");
    return false;
}


static void handleDescriptor(Sequencer *me, AOEvent const *evt)
{
    uint16_t sz = AOEvent_dataSize(evt);
    // TODO Check size.
    PulseTrain const *pt = (PulseTrain const *)AOEvent_data(evt);
    PulseTrain_print(pt, sz);
    // Scale amplitude 0..255 to 0..8160 mV (for now).
    BSP_setPrimaryVoltage_mV(PulseTrain_amplitude(pt) * 32);
    setPulseWidth(me, PulseTrain_pulseWidth(pt));
    Burst burst;
    Deltas deltas;
    startBurst(PulseTrain_getBurst(pt, &burst), PulseTrain_getDeltas(pt, sz, &deltas));
}


static bool enQueueDescriptor(Sequencer *me, AOEvent const *evt)
{
    if (EventQueue_repostEvent(&me->ptd_queue, evt)) {
        Sequencer_notifyPtQueue(me);
        return true;
    }

    return false;
}


static bool handleQueuedDescriptor(Sequencer *me)
{
    if (EventQueue_handleNextEvent(&me->ptd_queue, (EvtFunc)&handleDescriptor, me)) {
        Sequencer_notifyPtQueue(me);
        return true;
    }

    return false;
}


static void *stateStreaming(Sequencer *me, AOEvent const *evt)
{
    switch (AOEvent_type(evt))
    {
        case ET_AO_ENTRY:
            BSP_logf("Sequencer_%s ENTRY\n", __func__);
            handleQueuedDescriptor(me);
            break;
        case ET_AO_EXIT:
            BSP_logf("Sequencer_%s EXIT\n", __func__);
            break;
        case ET_QUEUE_PULSE_TRAIN:
            enQueueDescriptor(me, evt);
            break;
        case ET_SELECT_NEXT_PATTERN:
        case ET_SELECT_PATTERN_BY_NAME:
            // Ignore for now.
            break;
        case ET_BURST_STARTED:
            break;
        case ET_BURST_COMPLETED:
            // BSP_logf("Burst completed\n");
            break;
        case ET_BURST_EXPIRED:
            // BSP_logf("Burst expired\n");
            if (handleQueuedDescriptor(me)) break;
            return &stateIdle;                  // Otherwise transition.
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
            PatternIterator_init(&me->pi, &me->pattern_descr[me->pattern_index]);
            CLI_logf("Starting '%s'\n", me->pi.pattern_descr->name);
            return &statePulsing;               // Transition.
        case ET_QUEUE_PULSE_TRAIN:
            BSP_logf("Incoming pulse train of size %hu\n", AOEvent_dataSize(evt));
            if (enQueueDescriptor(me, evt)) {
                return &stateStreaming;         // Transition.
            }
            break;
        case ET_BURST_EXPIRED:
            CLI_logf("Finished '%s'\n", me->pi.pattern_descr->name);
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
            if (++me->pattern_index == me->nr_of_patterns) me->pattern_index = 0;
            switchPattern(me);
            return &stateIdle;                  // Transition.
        case ET_SELECT_PATTERN_BY_NAME:
            selectPatternByName(me, (char const *)AOEvent_data(evt), AOEvent_dataSize(evt));
            switchPattern(me);
            return &stateIdle;                  // Transition.
        case ET_TOGGLE_PLAY_PAUSE:
            CLI_logf("Resuming '%s'\n", me->pi.pattern_descr->name);
            // Fall through.
        case ET_PLAY:
            return &statePulsing;               // Transition.
        case ET_BURST_EXPIRED:
            if (PatternIterator_done(&me->pi)) {
                CLI_logf("Finished '%s'\n", me->pi.pattern_descr->name);
                return &stateIdle;              // Transition.
            }
            CLI_logf("Pausing '%s'\n", me->pi.pattern_descr->name);
            break;
        default:
            return stateCanopy(me, evt);        // Forward the event.
    }
    return NULL;
}


static uint8_t getPhase(uint8_t const elcon[2])
{
    M_ASSERT((elcon[0] & elcon[1]) == 0);       // Prevent shorts.
    return (elcon[0] & 0x5) ? 0 : 1;
}


static bool scheduleNextBurst(Sequencer *me)
{
    Burst burst;
    if (PatternIterator_getNextBurst(&me->pi, &burst)) {
        if (burst.pulse_width_¼_µs > MAX_PULSE_WIDTH_¼_µs) {
            burst.pulse_width_¼_µs = MAX_PULSE_WIDTH_¼_µs;
        }
        // BSP_logf("Pulse width is %hu µs\n", burstpulse_width_micros);
        burst.phase = getPhase(burst.elcon);
        Deltas deltas = {0};
        return startBurst(&burst, &deltas);
    }

    return false;
}


static void *statePulsing(Sequencer *me, AOEvent const *evt)
{
    switch (AOEvent_type(evt))
    {
        case ET_AO_ENTRY:
            BSP_logf("Sequencer_%s ENTRY\n", __func__);
            setPlayState(me, PS_PLAYING);
            scheduleNextBurst(me);
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
            if (! scheduleNextBurst(me)) {
                CLI_logf("Finished '%s'\n", me->pi.pattern_descr->name);
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
    EventQueue_init(&me->ptd_queue, me->ptd_storage, sizeof me->ptd_storage);
    return me;
}


Sequencer *Sequencer_init(Sequencer *me)
{
    me->state = &stateNop;
    me->pattern_descr = pattern_descriptors;
    me->nr_of_patterns = M_DIM(pattern_descriptors);
    me->pattern_index = 2;
    me->intensity_percent = 0;
    me->play_state = PS_UNKNOWN;
    BSP_registerPulseDelegate(&me->event_queue);
    return me;
}


void Sequencer_start(Sequencer *me)
{
    checkAllPatterns(me);
    me->state = stateIdle;
    me->state(me, AOEvent_newEntryEvent());
    setIntensityPercentage(me, DEFAULT_INTENSITY_PERCENT);
    BSP_primaryVoltageEnable(true);
}


bool Sequencer_handleEvent(Sequencer *me)
{
    return EventQueue_handleNextEvent(&me->event_queue, (EvtFunc)&dispatchEvent, me);
}


uint16_t Sequencer_getNrOfPatterns(Sequencer const *me)
{
    return me->nr_of_patterns;
}


void Sequencer_getPatternNames(Sequencer const *me, char const *names[], uint8_t cnt)
{
    if (cnt > me->nr_of_patterns) cnt = me->nr_of_patterns;
    for (uint8_t i = 0; i < cnt; i++) {
        names[i] = me->pattern_descr[i].name;
    }
}


uint8_t Sequencer_getIntensityPercentage(Sequencer const *me)
{
    return me->intensity_percent;
}


void Sequencer_notifyIntensity(Sequencer const *me)
{
    Attribute_changed(AI_INTENSITY_PERCENT, EE_UNSIGNED_INT_1, &me->intensity_percent, sizeof me->intensity_percent);
}


void Sequencer_notifyPattern(Sequencer const *me)
{
    PatternDescr const *pd = &me->pattern_descr[me->pattern_index];
    Attribute_changed(AI_CURRENT_PATTERN_NAME, EE_UTF8_1LEN, (uint8_t const *)pd->name, strlen(pd->name));
}


void Sequencer_notifyPlayState(Sequencer const *me)
{
    Attribute_changed(AI_PLAY_PAUSE_STOP, EE_UNSIGNED_INT_1, &me->play_state, sizeof me->play_state);
}


void Sequencer_notifyPtQueue(Sequencer const *me)
{
    uint16_t nr_of_bytes_free = EventQueue_availableSpace(&me->ptd_queue) - AOEvent_minimumSize();
    Attribute_changed(AI_PT_DESCRIPTOR_QUEUE, EE_UNSIGNED_INT_2, (uint8_t const *)&nr_of_bytes_free, sizeof nr_of_bytes_free);
}


void Sequencer_stop(Sequencer *me)
{
    BSP_primaryVoltageEnable(false);
    me->state(me, AOEvent_newExitEvent());
    me->state = stateNop;
}


void Sequencer_delete(Sequencer *me)
{
    free(me);
}
