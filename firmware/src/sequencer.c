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
#include "debug_cli.h"
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
    PatternDescr const *pattern_descr;
    uint16_t nr_of_patterns;
    PatternIterator pi;
    uint8_t pattern_index;
};

enum { EL_0, EL_A, EL_B, EL_C = 4, EL_AC = (EL_A | EL_C), EL_D = 8, EL_BD = (EL_B | EL_D) };

/*static*/ char const elset_str[][5] = {
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

static PatternDescr const pattern_descriptors[] =
{
    {
        .name = "Jackhammer",
        .pattern = pattern_jackhammer,
        .nr_of_elcons = M_DIM(pattern_jackhammer),
        .pace_ms = 125,
        .nr_of_steps = 3,
        .nr_of_reps = 200,
    },
    {
        .name = "Toggle",
        .pattern = pattern_toggle,
        .nr_of_elcons = M_DIM(pattern_toggle),
        .pace_ms = 50,
        .nr_of_steps = 5,
        .nr_of_reps = 300,
    },
    {
        .name = "CrossToggle",
        .pattern = pattern_cross_toggle,
        .nr_of_elcons = M_DIM(pattern_cross_toggle),
        .pace_ms = 20,
        .nr_of_steps = 5,
        .nr_of_reps = 200,
    },
    {
        .name = "Circle",
        .pattern = pattern_circle,
        .nr_of_elcons = M_DIM(pattern_circle),
        .pace_ms = 30,
        .nr_of_steps = 9,
        .nr_of_reps = 40,
    },
};


static void selectNextRoutine(Sequencer *me)
{
    BSP_setPrimaryVoltage_mV(DEFAULT_PRIMARY_VOLTAGE_mV);
    if (++me->pattern_index == me->nr_of_patterns) me->pattern_index = 0;
    PatternDescr const *pd = &me->pattern_descr[me->pattern_index];
    CLI_logf("Switching to '%s'\n", pd->name);
    PatternIterator_init(&me->pi, pd);
}


static void printAdcValues(uint16_t const *v)
{
    uint32_t Iprim_mA = ((uint32_t)v[0] * 2063UL) /  1024;
    uint32_t Vcap_mV = ((uint32_t)v[1] * 52813UL) / 16384;
    uint32_t Vbat_mV = ((uint32_t)v[2] * 52813UL) / 16384;
    CLI_logf("Iprim=%u mA, Vcap=%u mV, Vbat=%u mV\n", Iprim_mA, Vcap_mV, Vbat_mV);
}


static void *stateNop(Sequencer *me, AOEvent const *evt)
{
    BSP_logf("Sequencer_%s unexpected event: %u\n", __func__, AOEvent_type(evt));
    return NULL;
}

// Forward declaration.
static void *statePulsing(Sequencer *, AOEvent const *);


static void *stateIdle(Sequencer *me, AOEvent const *evt)
{
    switch (AOEvent_type(evt))
    {
        case ET_AO_ENTRY:
            BSP_logf("Sequencer_%s ENTRY\n", __func__);
            break;
        case ET_AO_EXIT:
            BSP_logf("Sequencer_%s EXIT\n", __func__);
            break;
        case ET_ADC_DATA_AVAILABLE:
            printAdcValues((uint16_t const *)AOEvent_data(evt));
            break;
        case ET_PLAY_PAUSE:
            PatternIterator_init(&me->pi, &me->pattern_descr[me->pattern_index]);
            CLI_logf("Starting '%s'\n", me->pi.pattern_descr->name);
            return &statePulsing;               // Transition.
        case ET_NEXT_ROUTINE:
            selectNextRoutine(me);
            break;
        case ET_BURST_COMPLETED:
            BSP_logf("Pulse train last pulse done\n");
            break;
        case ET_BURST_EXPIRED:
            CLI_logf("Finished '%s'\n", me->pi.pattern_descr->name);
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
        case ET_PLAY_PAUSE:
            CLI_logf("Resuming '%s'\n", me->pi.pattern_descr->name);
            return &statePulsing;
        case ET_NEXT_ROUTINE:
            selectNextRoutine(me);
            break;
        case ET_BURST_COMPLETED:
            // BSP_logf("Last pulse done\n");
            break;
        case ET_BURST_EXPIRED:
            if (PatternIterator_done(&me->pi)) {
                CLI_logf("Finished '%s'\n", me->pi.pattern_descr->name);
                return &stateIdle;              // Transition.
            }
            CLI_logf("Pausing '%s'\n", me->pi.pattern_descr->name);
            break;
        case ET_ADC_DATA_AVAILABLE:
            printAdcValues((uint16_t const *)AOEvent_data(evt));
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
        case ET_PLAY_PAUSE:
            return &statePaused;                // Transition.
        case ET_NEXT_ROUTINE:
            selectNextRoutine(me);
            break;
        case ET_BURST_STARTED:
            // BSP_logf("Pulse train started\n");
            break;
        case ET_BURST_COMPLETED:
            // BSP_logf("Last pulse done\n");
            break;
        case ET_BURST_EXPIRED:
            if (! scheduleNextPulseTrain(me)) {
                CLI_logf("Finished '%s'\n", me->pi.pattern_descr->name);
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
    return me;
}


Sequencer *Sequencer_init(Sequencer *me)
{
    me->state = &stateNop;
    me->pattern_descr = pattern_descriptors;
    me->nr_of_patterns = M_DIM(pattern_descriptors);
    me->pattern_index = 2;
    BSP_registerPulseDelegate(&me->event_queue);
    return me;
}


void Sequencer_start(Sequencer *me)
{
    for (uint16_t i = 0; i < me->nr_of_patterns; i++) {
        PatternDescr const *pd = &me->pattern_descr[i];
        BSP_logf("Checking '%s'\n", pd->name);
        PatternIterator_checkPattern(pd->pattern, pd->nr_of_elcons);
    }

    PatternIterator_init(&me->pi, &me->pattern_descr[me->pattern_index]);
    uint32_t total_nr_of_pulses = 0;
    PulseTrain pt;
    while (PatternIterator_getNextPulseTrain(&me->pi, &pt)) {
        // BSP_logf(" %3hu * {%s, %s}\n", pt.nr_of_pulses, elset_str[pt.elcon[0]], elset_str[pt.elcon[1]]);
        total_nr_of_pulses += pt.nr_of_pulses;
    }
    BSP_logf("Total number of pulses: %u\n", total_nr_of_pulses);

    PatternIterator_init(&me->pi, &me->pattern_descr[me->pattern_index]);
    me->state = stateIdle;
    me->state(me, AOEvent_newEntryEvent());
}


bool Sequencer_handleEvent(Sequencer *me)
{
    return EventQueue_handleNextEvent(&me->event_queue, (EvtFunc)&dispatchEvent, me);
}


uint16_t Sequencer_nrOfPatterns(Sequencer const *me)
{
    return me->nr_of_patterns;
}


void Sequencer_getPatternNames(Sequencer const *me, char const *names[], uint16_t cnt)
{
    if (cnt > me->nr_of_patterns) cnt = me->nr_of_patterns;
    for (uint16_t i = 0; i < cnt; i++) {
        names[i] = me->pattern_descr[i].name;
    }
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
