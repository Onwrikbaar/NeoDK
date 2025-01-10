/*
 * pattern_iter.c
 *
 *  NOTICE (do not remove):
 *      This file is part of project NeoDK (https://github.com/Onwrikbaar/NeoDK).
 *      See https://github.com/Onwrikbaar/NeoDK/blob/main/LICENSE.txt for full license details.
 *
 *  Created on: 6 Mar 2024
 *      Author: mark
 *   Copyright  2024, 2025 Neostim™
 */

#include <stdbool.h>
#include "bsp_dbg.h"

// This module implements:
#include "pattern_iter.h"


static void nextElconIfLastStep(PatternIterator *me)
{
    PatternDescr const *pd = me->pattern_descr;
    if (me->step_nr == pd->nr_of_steps - 1) {   // Transition done.
        me->step_nr = 0;
        if (++me->elcon_nr == pd->nr_of_elcons) {
            me->elcon_nr = 0;                   // Wrap around.
            me->nr_of_reps -= 1;
        }
    }
}


static uint8_t const *getNextPattern(PatternIterator *me, uint16_t *nr_of_pulses)
{
    PatternDescr const *pd = me->pattern_descr;
    uint8_t elcon_nr = me->elcon_nr;
    if (me->segment_nr == 0) {
        *nr_of_pulses = pd->nr_of_steps - me->step_nr - 1;
        me->segment_nr = 1;
    } else {
        if (++elcon_nr == pd->nr_of_elcons) elcon_nr = 0;
        *nr_of_pulses = ++me->step_nr;
        nextElconIfLastStep(me);
        me->segment_nr = 0;
    }
    return pd->pattern[elcon_nr];
}

/*
 * Below are the functions implementing this module's interface.
 */

bool PatternIterator_checkPattern(uint8_t const pattern[][2], uint16_t nr_of_elcons)
{
    for (uint16_t i = 0; i < nr_of_elcons; i++) {
        uint8_t const *elcon = pattern[i];
        if ((elcon[0] & elcon[1]) != 0) {
            BSP_logf("%s: short in elcon %hu\n", __func__, i);
            return false;
        }
    }
    return true;
}


void PatternIterator_init(PatternIterator *me, PatternDescr const *pd)
{
    me->pattern_descr = pd;
    me->nr_of_reps = pd->nr_of_reps;
    me->elcon_nr = 0;
    M_ASSERT(pd->nr_of_steps != 0);
    me->step_nr = 0;
    me->segment_nr = 0;                         // 0 or 1.
}


bool PatternIterator_done(PatternIterator *me)
{
    return me->nr_of_reps == 0;
}


bool PatternIterator_getNextBurst(PatternIterator *me, Burst *burst)
{
    if (PatternIterator_done(me)) return false;

    uint8_t const *elcon = getNextPattern(me, &burst->nr_of_pulses);
    burst->elcon[0] = elcon[0];
    burst->elcon[1] = elcon[1];
    burst->pulse_width_¼_µs = me->pulse_width_micros * 4;
    burst->pace_µs = me->pattern_descr->pace_µs;
    return true;
}
