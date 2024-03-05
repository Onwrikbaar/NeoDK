/*
 * sequencer.c -- the pulse train executor
 *
 *  Created on: 27 Feb 2024
 *      Author: mark
 *   Copyright  2024 Neostim
 */

#include <stdlib.h>

#include "bsp_dbg.h"
#include "bsp_mao.h"
#include "bsp_app.h"
#include "app_event.h"
#include "convenience.h"

// This module implements:
#include "sequencer.h"

#define MAX_PULSE_WIDTH_MICROS           200

enum { EL_A = 1, EL_B = 2, EL_C = 4, EL_AC = (EL_A | EL_C), EL_D = 8, EL_BD = (EL_B | EL_D) };

typedef struct {
    uint8_t const (*pattern)[2];
    uint16_t nr_of_elcons;
    uint16_t elcon_nr;
    uint8_t pulse_width_micros;
} PatternIterator;

typedef void *(*StateFunc)(Sequencer *, uint32_t);

struct _Sequencer {
    StateFunc state;
    PatternIterator pi;
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
    {EL_AC, EL_D,},
    {EL_D,  EL_A},
    {EL_A,  EL_BD},
    {EL_BD, EL_AC},
};


static void checkPattern(uint8_t const pattern[][2], uint16_t nr_of_elcons)
{
    for (uint16_t i = 0; i < nr_of_elcons; i++) {
        uint8_t const *elcon = pattern[i];
        if ((elcon[0] & elcon[1]) != 0) {
            BSP_logf("%s: error in elcon %hu\n", __func__, i);
        }
    }
}


static void PatternIterator_init(PatternIterator *me, uint8_t const pattern[][2], uint16_t nr_of_elcons)
{
    me->pattern = pattern;
    me->nr_of_elcons = nr_of_elcons;
    me->elcon_nr = 0;
    me->pulse_width_micros = 60;
}


static bool PatternIterator_done(PatternIterator *me)
{
    return me->elcon_nr == me->nr_of_elcons;
}


static bool PatternIterator_getNextPulseTrain(PatternIterator *me, PulseTrain *pt)
{
    if (PatternIterator_done(me)) return false;

    pt->elcon[0] = me->pattern[me->elcon_nr][0];
    pt->elcon[1] = me->pattern[me->elcon_nr][1];
    pt->pulse_width_micros = me->pulse_width_micros;
    pt->pace_ms = 20;                           // For 1000/pace pulses per second.
    pt->nr_of_pulses = 4;

    me->elcon_nr++;
    return true;
}

// Forward declaration.
static void *statePulsing(Sequencer *me, uint32_t evt);


static void *stateIdle(Sequencer *me, uint32_t evt)
{
    switch (evt)
    {
        case ET_AO_ENTRY:
            BSP_logf("%s ENTRY\n", __func__);
            // TODO Choose a different pattern each time?
            PatternIterator_init(&me->pi, pattern_circle, M_DIM(pattern_circle));
            break;
        case ET_AO_EXIT:
            BSP_logf("%s EXIT\n", __func__);
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
            BSP_logf("Sequencer_%s unexpected event: %u\n", __func__, evt);
    }
    return NULL;
}


static void *statePaused(Sequencer *me, uint32_t evt)
{
    switch (evt)
    {
        case ET_AO_ENTRY:
            BSP_logf("%s ENTRY\n", __func__);
            break;
        case ET_AO_EXIT:
            BSP_logf("%s EXIT\n", __func__);
            break;
        case ET_SEQUENCER_PLAY_PAUSE:
            return &statePulsing;
        case ET_BURST_COMPLETED:
            BSP_logf("Pulse train last pulse done\n");
            break;
        case ET_BURST_EXPIRED:
            BSP_logf("Pulse train finished\n");
            if (PatternIterator_done(&me->pi)) {
                return &stateIdle;              // Transition.
            }
            break;
        default:
            BSP_logf("Sequencer_%s unexpected event: %u\n", __func__, evt);
    }
    return NULL;
}


static void *statePulsing(Sequencer *me, uint32_t evt)
{
    switch (evt)
    {
        case ET_AO_ENTRY: {
            BSP_logf("%s ENTRY\n", __func__);
            PulseTrain pt;
            if (PatternIterator_getNextPulseTrain(&me->pi, &pt)) {
                BSP_logf("Pulse width is %hhu µs\n", pt.pulse_width_micros);
                BSP_startPulseTrain(&pt);
            }
            break;
        }
        case ET_AO_EXIT:
            BSP_logf("%s EXIT\n", __func__);
            break;
        case ET_SEQUENCER_PLAY_PAUSE:
            return &statePaused;
        case ET_BURST_STARTED:
            BSP_logf("Pulse train started\n");
            break;
        case ET_BURST_COMPLETED:
            BSP_logf("Pulse train last pulse done\n");
            break;
        case ET_BURST_EXPIRED: {
            PulseTrain pt;
            if (! PatternIterator_getNextPulseTrain(&me->pi, &pt)) {
            BSP_logf("Pulse train finished; no more pulse trains\n");
                return &stateIdle;              // Transition.
            }
            BSP_logf("Pulse train finished; schedule next one\n\n");
            BSP_logf("Pulse width is %hhu µs\n", pt.pulse_width_micros);
            BSP_startPulseTrain(&pt);
            break;
        }
        default:
            BSP_logf("Sequencer_%s unexpected event: %u\n", __func__, evt);
    }
    return NULL;
}


static void handleEvent(Sequencer *me, uint32_t evt)
{
    // BSP_logf("%s(%u)\n", __func__, evt);
    BSP_criticalSectionEnter();
    StateFunc new_state = me->state(me, evt);
    if (new_state != NULL) {                    // Transition.
        me->state(me, ET_AO_EXIT);
        me->state = new_state;
        me->state(me, ET_AO_ENTRY);
    }
    BSP_criticalSectionExit();
}

/*
 * Below are the functions implementing this module's interface.
 */

Sequencer *Sequencer_new()
{
    Sequencer *me = (Sequencer *)malloc(sizeof(Sequencer));
    me->state = &stateIdle;

    Selector sel;
    BSP_registerPulseHandler(Selector_init(&sel, (Action)&handleEvent, me));
    return me;
}


void Sequencer_start(Sequencer *me)
{
    checkPattern(pattern_simple, M_DIM(pattern_simple));
    checkPattern(pattern_circle, M_DIM(pattern_circle));
    handleEvent(me, ET_AO_ENTRY);
}


void Sequencer_togglePlayPause(Sequencer *me, uint32_t button_pushed)
{
    if (button_pushed) {
        handleEvent(me, ET_SEQUENCER_PLAY_PAUSE);
    }
}


void Sequencer_delete(Sequencer *me)
{
    BSP_disableOutputStage();
    free(me);
}
