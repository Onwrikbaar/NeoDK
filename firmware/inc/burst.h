/*
 * burst.h -- the interface to the Burst (aka pulse train) object.
 *
 *  NOTICE (do not remove):
 *      This file is part of project NeoDK (https://github.com/Onwrikbaar/NeoDK).
 *      See https://github.com/Onwrikbaar/NeoDK/blob/main/LICENSE.txt for full license details.
 *
 *  Created on: 29 Dec 2019
 *      Author: mark
 *   Copyright  2019..2024 Neostim™
 */

#ifndef INC_BURST_H_
#define INC_BURST_H_

#include <stdint.h>

typedef struct _Burst Burst;                    // Opaque type.
typedef uint8_t burst_size_t;
typedef uint8_t burst_phase_t;

// Burst methods.
burst_size_t Burst_size();
Burst *Burst_new(void *addr, burst_size_t);
Burst *Burst_copy(void *addr, burst_size_t, Burst const *original);
Burst *Burst_setSequenceNumber(Burst *, uint8_t sequence_number);
Burst *Burst_init(Burst *, uint8_t electrode_set[2], uint16_t nr_of_pulses);
void   Burst_setPhase(Burst *, burst_phase_t phase);
burst_phase_t Burst_phase(Burst const *);
Burst *Burst_setStartTimeMicros(Burst *, uint32_t start_time_micros);
uint16_t Burst_nrOfPulses(Burst const *);
void   Burst_print(Burst const *);

#endif
