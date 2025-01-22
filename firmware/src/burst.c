/*
 * burst.c
 *
 *  NOTICE (do not remove):
 *      This file is part of project NeoDK (https://github.com/Onwrikbaar/NeoDK).
 *      See https://github.com/Onwrikbaar/NeoDK/blob/main/LICENSE.txt for full license details.
 *
 *  Created on: 9 Jan 2025
 *      Author: mark
 *   Copyright  2025 Neostim™
 */

#include "bsp_dbg.h"

// This module implements:
#include "burst.h"


bool Burst_isValid(Burst const *me)
{
    // Pulse repetition rate must be in [16..200] Hz.
    if (me->pace_µs < (MIN_PULSE_PACE_µs)) return false;         
    if (me->pace_µs > (MAX_PULSE_PACE_µs)) return false;
    if (me->pulse_width_¼_µs < MIN_PULSE_WIDTH_¼_µs) return false;
    if (me->nr_of_pulses == 0) return false;
    return true;
}


uint32_t Burst_duration_µs(Burst const *me)
{
    return me->nr_of_pulses * me->pace_µs;
}


uint8_t Burst_pulseWidth_µs(Burst const *me)
{
    return (me->pulse_width_¼_µs + 2) / 4;
}


void Burst_applyDeltas(Burst *me, Deltas const *deltas)
{
    int32_t new_pw = me->pulse_width_¼_µs + deltas->delta_width_¼_µs;
    if (new_pw < MIN_PULSE_WIDTH_¼_µs) new_pw = MIN_PULSE_WIDTH_¼_µs;
    if (new_pw > MAX_PULSE_WIDTH_¼_µs) new_pw = MAX_PULSE_WIDTH_¼_µs;
    me->pulse_width_¼_µs = new_pw;

    int32_t new_pace = me->pace_µs + deltas->delta_pace_µs;
    if (new_pace < MIN_PULSE_PACE_µs) new_pace = MIN_PULSE_PACE_µs;
    else if (new_pace > MAX_PULSE_PACE_µs) new_pace = MAX_PULSE_PACE_µs;
    me->pace_µs = new_pace;
}


void Burst_print(Burst const *me)
{
    BSP_logf("Burst: t=%u µs, ec=0x%x<>0x%x, phase=%hhu, np=%hu, pace=%hu µs, amp=%hhu, pw=%hhu µs\n",
            me->start_time_µs, me->elcon[0], me->elcon[1], me->phase,
            me->nr_of_pulses, me->pace_µs, me->amplitude, Burst_pulseWidth_µs(me));
}
