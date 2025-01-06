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

#include <string.h>

#include "bsp_dbg.h"
#include "convenience.h"

// This module implements:
#include "pulse_train.h"

/**
 * This structure specifies a pulse train's polarity, intensity, timing and electrode configuration.
 * Multi-byte members are little-Endian.
 */
struct _PulseTrain {
    uint8_t  meta;                  // Type, version, flags, etc., for correct interpretation of this descriptor.
    uint8_t  sequence_number;       // For diagnostics. Wraps around to 0 after 255.
    uint8_t  phase;                 // Bits 2..1 select 1 of 4 biphasic output stages. Bit 0 is the selected stage's polarity.
    uint8_t  amplitude;             // Voltage, current or power, in units of 1/255 of the set maximum.
    uint32_t start_time_µs;         // When this burst should begin, in microseconds since 'now'.
    uint8_t  electrode_set[2];      // The (max 8) electrodes connected to each of the two output phases.
    uint8_t  pulse_width_µs;        // The duration of one pulse [µs].
    uint8_t  pace_ms;               // [1..255] milliseconds between the start of consecutive pulses.
    uint16_t nr_of_pulses;          // Length of this burst.
    uint8_t  nr_of_same_phase;      // Bits 4..0: number of pulses before changing polarity (max 31, 0 means never).
    // The following three members [-128..127] are applied after each pulse.
    int8_t   delta_pulse_width_¼_µs;// [0.25 µs]. Changes the duration of a pulse.
    int8_t   delta_pace_µs;         // [µs]. Modifies the time between pulses.
    int8_t   delta_amplitude;       // In units of 1/255 of the set maximum.
    // BLE 4.0 / 4.1 guaranteed ATT payload size is 20 bytes, so we have a couple to spare.
    uint8_t  reserved_for_future_use[2];
};

/*
static void applyDeltaPace(PulseTrain *me)
{
    int16_t new_pace_ms = me->pace_ms + me->delta_pace_µs;
    if (new_pace_ms < 0) new_pace_ms = 0;
    else if (new_pace_ms > 255) new_pace_ms = 255;
    me->pace_ms = new_pace_ms;
}
*/
/*
 * Below are the functions implementing this module's interface.
 */

pulse_train_size_t PulseTrain_size()
{
    return sizeof(PulseTrain);
}


PulseTrain *PulseTrain_copy(void *addr, pulse_train_size_t nb, PulseTrain const *original)
{
    if (addr == NULL || nb < PulseTrain_size()) return NULL;
    if (original == NULL) return memset(addr, 0, nb);
    return memcpy(addr, original, PulseTrain_size());
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
    me->pace_ms = burst->pace_ms;
    me->nr_of_pulses = burst->nr_of_pulses;
    me->pulse_width_µs = (burst->pulse_width_¼_µs + 2) / 4;
    return me;
}


uint16_t PulseTrain_amplitude(PulseTrain const *me)
{
    return me->amplitude;
}


uint8_t PulseTrain_pulseWidth(PulseTrain const *me)
{
    return me->pulse_width_µs;
}


Burst const *PulseTrain_getBurst(PulseTrain const *me, Burst *burst)
{
    burst->elcon[0] = me->electrode_set[0];
    burst->elcon[1] = me->electrode_set[1];
    burst->phase = me->phase;
    burst->pace_ms = me->pace_ms;
    burst->pulse_width_¼_µs = me->pulse_width_µs * 4;
    burst->nr_of_pulses = me->nr_of_pulses;
    return burst;
}


void PulseTrain_getDeltas(PulseTrain const *me, Deltas *deltas)
{
    deltas->delta_width_¼_µs = me->delta_pulse_width_¼_µs;
    deltas->delta_pace_µs    = me->delta_pace_µs;
    deltas->delta_amplitude  = me->delta_amplitude;
}


void PulseTrain_clearDeltas(PulseTrain *deltas)
{
    deltas->delta_pulse_width_¼_µs = 0;
    deltas->delta_pace_µs          = 0;
    deltas->delta_amplitude        = 0;
}


void PulseTrain_setDeltas(PulseTrain *me, int16_t delta_width_¼_µs, int8_t delta_amplitude, int8_t delta_pace_µs)
{
    me->delta_pulse_width_¼_µs = delta_width_¼_µs;
    me->delta_amplitude        = delta_amplitude;
    me->delta_pace_µs          = delta_pace_µs;
}


void PulseTrain_print(PulseTrain const *me)
{
    BSP_logf("Pt %3hhu: t=%u µs, ec=0x%x<>0x%x, phase=%hhu, np=%hu, pace=%hhu ms, amp=%hhu, pw=%hhu µs, Δ=%hhd ¼µs\n",
            me->sequence_number, me->start_time_µs, me->electrode_set[0], me->electrode_set[1],
            me->phase, me->nr_of_pulses, me->pace_ms, me->amplitude, me->pulse_width_µs, me->delta_pulse_width_¼_µs);
}
