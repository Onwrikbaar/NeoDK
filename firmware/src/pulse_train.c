/*
 * pulse_train.c
 *
 *  NOTICE (do not remove):
 *      This file is part of project NeoDK (https://github.com/Onwrikbaar/NeoDK).
 *      See https://github.com/Onwrikbaar/NeoDK/blob/main/LICENSE.txt for full license details.
 *
 *  Created on: 29 Dec 2019
 *      Author: mark
 *   Copyright  2019..2025 Neostim™
 */

#include <stddef.h>
#include <string.h>

#include "bsp_dbg.h"
#include "convenience.h"

// This module implements:
#include "pulse_train.h"

/**
 * This 16-byte structure specifies a pulse train's output stage, polarity, electrode configuration, timing and intensity.
 * Multi-byte members are little-Endian.
 */
struct _PulseTrain {
    uint8_t  meta;                  // Type, version, flags, etc., for correct interpretation of this descriptor.
    uint8_t  sequence_number;       // For diagnostics. Wraps around to 0 after 255.
    uint8_t  phase;                 // Bits 2..1 select 1 of 4 biphasic output stages. Bit 0 is the selected stage's polarity.
    uint8_t  pulse_width_µs;        // The duration of one pulse [µs].
    uint32_t start_time_µs;         // When this burst should begin, relative to the start of the stream.
    uint8_t  electrode_set[2];      // The (max 8) electrodes connected to each of the two output phases.
    uint16_t nr_of_pulses;          // Length of this burst.
    uint8_t  pace_¼ms;              // [0.25 ms] time between the start of consecutive pulses.
    uint8_t  amplitude;             // Voltage, current or power, in units of 1/255 of the set maximum.
    // The following members [-128..127] are applied after each pulse of this train.
    int8_t   delta_pulse_width_¼µs; // [0.25 µs]. Changes the duration of the pulses.
    int8_t   delta_pace_µs;         // [µs]. Modifies the time between pulses.
//  int8_t   delta_amplitude;       // Under consideration.
};

/*
 * Below are the functions implementing this module's interface.
 */

uint16_t PulseTrain_size()
{
    return sizeof(PulseTrain);
}


PulseTrain *PulseTrain_copy(void *addr, uint16_t nb, PulseTrain const *original)
{
    if (addr == NULL || nb < PulseTrain_size()) return NULL;
    if (original == NULL) return (PulseTrain *)memset(addr, 0, nb);
    return (PulseTrain *)memcpy(addr, original, PulseTrain_size());
}


PulseTrain *PulseTrain_init(PulseTrain *me, uint8_t seq_nr, uint32_t timestamp, Burst const *burst)
{
    me->meta = 0;
    me->sequence_number = seq_nr;
    me->start_time_µs = timestamp;
    me->electrode_set[0] = burst->elcon[0];
    me->electrode_set[1] = burst->elcon[1];
    me->phase = burst->phase;
    me->amplitude = 0;
    me->pace_¼ms = burst->pace_µs / 250;
    me->nr_of_pulses = burst->nr_of_pulses;
    me->pulse_width_µs = Burst_pulseWidth_µs(burst);
    return me;
}


bool PulseTrain_isValid(PulseTrain const *me, uint16_t sz)
{
    // TODO More checks.
    return sz >= 10 && sz <= sizeof(PulseTrain) && me->meta == 0x00;
}


uint32_t PulseTrain_timestamp(PulseTrain const *me)
{
    return me->start_time_µs;
}


uint8_t PulseTrain_phase(PulseTrain const *me)
{
    return me->phase & 0x7;
}


uint8_t PulseTrain_pulseWidth(PulseTrain const *me)
{
    return me->pulse_width_µs;
}


uint16_t PulseTrain_amplitude(PulseTrain const *me)
{
    return me->amplitude;
}


Burst const *PulseTrain_getBurst(PulseTrain const *me, Burst *burst)
{
    burst->start_time_µs = me->start_time_µs;
    burst->elcon[0] = me->electrode_set[0];
    burst->elcon[1] = me->electrode_set[1];
    burst->phase = PulseTrain_phase(me);
    burst->pace_µs = me->pace_¼ms * 250;
    burst->pulse_width_¼µs = me->pulse_width_µs * 4;
    burst->nr_of_pulses = me->nr_of_pulses;
    burst->amplitude = me->amplitude;
    return burst;
}


void PulseTrain_clearDeltas(PulseTrain *me)
{
    // BSP_logf("%s\n", __func__);
    me->delta_pulse_width_¼µs = 0;
    me->delta_pace_µs         = 0;
}


void PulseTrain_setDeltas(PulseTrain *me, int8_t delta_width_¼µs, int8_t delta_pace_µs)
{
    me->delta_pulse_width_¼µs = delta_width_¼µs;
    me->delta_pace_µs         = delta_pace_µs;
}


Deltas const *PulseTrain_getDeltas(PulseTrain const *me, uint16_t sz, Deltas *deltas)
{
    deltas->delta_width_¼µs = sz > offsetof(PulseTrain, delta_pulse_width_¼µs) ? me->delta_pulse_width_¼µs : 0;
    deltas->delta_pace_µs   = sz > offsetof(PulseTrain, delta_pace_µs) ? me->delta_pace_µs : 0;
    return deltas;
}


void PulseTrain_print(PulseTrain const *me, uint16_t sz)
{
    BSP_logf("Pt %3hhu: t=%u µs, ec=0x%x<>0x%x, phase=%c, np=%2hu, pace=%hhu ¼ms, amp=%hhu, pw=%3hhu µs, Δ=%hhd ¼µs\n",
            me->sequence_number, me->start_time_µs, me->electrode_set[0], me->electrode_set[1],
            '0' + me->phase, me->nr_of_pulses, me->pace_¼ms, me->amplitude, me->pulse_width_µs,
            sz > offsetof(PulseTrain, delta_pulse_width_¼µs) ? me->delta_pulse_width_¼µs : 0);
}
