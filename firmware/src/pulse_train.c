/*
 * pulse_train.c
 *
 *  NOTICE (do not remove):
 *      This file is part of project NeoDK (https://github.com/Onwrikbaar/NeoDK).
 *      See https://github.com/Onwrikbaar/NeoDK/blob/main/LICENSE.txt for full license details.
 *
 *  Created on: 29 Dec 2019
 *      Author: mark
 *   Copyright  2019..2025 Neostim
 */

#include <string.h>

#include "bsp_dbg.h"
#include "convenience.h"
#include "pulse_train.h"

/**
 * This structure specifies a pulse train's polarity, intensity, timing and electrode configuration.
 * Multi-byte members are little-Endian.
 */
struct _PulseTrain {
    uint8_t  meta;              // Type, version, flags, etc., for correct interpretation of this descriptor.
    uint8_t  sequence_number;   // For diagnostics. Wraps around to 0 after 255.
    uint8_t  phase;             // Bits 2..1 select 1 of 4 biphasic output stages. Bit 0 is the selected stage's polarity.
    uint8_t  amplitude;         // Voltage, current or power, in units of 1/255 of the set maximum.
    uint32_t start_time_micros; // When this burst should begin, in microseconds since start of program.
    uint8_t  electrode_set[2];  // The (max 8) electrodes connected to each of the two output phases.
    uint8_t  pulse_width;       // The duration of one pulse [µs].
    uint8_t  pace_ms;           // [1..255] milliseconds between the start of consecutive pulses.
    uint16_t nr_of_pulses;      // Length of this burst.
    uint8_t  nr_of_same_phase;  // Bits 4..0: number of pulses before changing polarity (max 31, 0 means never).
    // The following three members [-128..127] are applied after each pulse.
    int8_t   delta_width_micros;// [µs]. Changes the duration of a pulse.
    int8_t   delta_amplitude;   // In units of 1/255 of the set maximum.
    int8_t   delta_pace_micros; // [µs]. Modifies the time between pulses.
    // BLE 4.0 / 4.1 guaranteed ATT payload size is 20 bytes, so we have a couple to spare.
    uint8_t  reserved_for_future_use[2];
};


static burst_phase_t const phase_mask = 0x1e;

/*
static void PulseTrain_togglePhase(PulseTrain *me)
{
    me->phase ^= 1;
}
*/

static void applyDeltaWidth(PulseTrain *me)
{
    int16_t new_width = me->pulse_width + me->delta_width_micros;
    if (new_width < 0) new_width = 0;
    else if (new_width > 255) new_width = 255;
    me->pulse_width = new_width;
}


static void applyDeltaAmplitude(PulseTrain *me)
{
    int16_t new_amplitude = me->amplitude + me->delta_amplitude;
    if (new_amplitude < 0) new_amplitude = 0;
    else if (new_amplitude > 255) new_amplitude = 255;
    me->amplitude = new_amplitude;
}


static void applyDeltaPace(PulseTrain *me)
{
    int16_t new_pace_ms = me->pace_ms + me->delta_pace_micros;
    if (new_pace_ms < 0) new_pace_ms = 0;
    else if (new_pace_ms > 255) new_pace_ms = 255;
    me->pace_ms = new_pace_ms;
}

/*
 * Below are the functions implementing this module's interface.
 */

burst_size_t PulseTrain_size()
{
    return sizeof(PulseTrain);
}


PulseTrain *PulseTrain_new(void *addr, burst_size_t nb)
{
    return PulseTrain_copy(addr, nb, NULL);
}


PulseTrain *PulseTrain_copy(void *addr, burst_size_t nb, PulseTrain const *original)
{
    if (addr == NULL || nb < PulseTrain_size()) return NULL;
    if (original == NULL) return memset(addr, 0, nb);
    return memcpy(addr, original, PulseTrain_size());
}


void PulseTrain_setPhase(PulseTrain *me, burst_phase_t phase)
{
    me->phase &= ~phase_mask;
    me->phase |= (phase & phase_mask);
}


burst_phase_t PulseTrain_phase(PulseTrain const *me)
{
    return me->phase & phase_mask;
}


PulseTrain *PulseTrain_init(PulseTrain *me, uint8_t electrode_set[2], uint16_t nr_of_pulses)
{
    me->meta = 0;
    me->phase = 0;
    me->amplitude = 200;
    me->electrode_set[0] = electrode_set[0];
    me->electrode_set[1] = electrode_set[1];
    me->pulse_width = 40;
    me->pace_ms = 25;
    me->nr_of_pulses = nr_of_pulses;
    return me;
}


PulseTrain *PulseTrain_setStartTimeMicros(PulseTrain *me, uint32_t start_time_micros)
{
    me->start_time_micros = start_time_micros;
    return me;
}


void PulseTrain_print(PulseTrain const *me)
{
    BSP_logf("PulseTrain %hhu: start=%u µs, np=%hu, pat=0x%x<>0x%x\n", me->sequence_number, me->start_time_micros, me->nr_of_pulses, me->electrode_set[0], me->electrode_set[1]);
}


uint16_t PulseTrain_nrOfPulses(PulseTrain const *me)
{
    return me->nr_of_pulses;
}


PulseTrain *PulseTrain_applyDeltas(PulseTrain *me)
{
    applyDeltaWidth(me);
    applyDeltaAmplitude(me);
    applyDeltaPace(me);
    return me;
}

// This one doesn't really belong here.
void PulseTrain_iterate(PulseTrain *bursts, uint16_t nob, void (*f)(PulseTrain const *))
{
    for (uint16_t i = 0; i < nob; i++) {
        f(&bursts[i]);
    }
}
