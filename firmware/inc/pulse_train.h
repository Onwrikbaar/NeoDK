/*
 * pulse_train.h -- the interface to the PulseTrain object.
 *
 *  NOTICE (do not remove):
 *      This file is part of project NeoDK (https://github.com/Onwrikbaar/NeoDK).
 *      See https://github.com/Onwrikbaar/NeoDK/blob/main/LICENSE.txt for full license details.
 *
 *  Created on: 29 Dec 2019
 *      Author: mark
 *   Copyright  2019..2025 Neostim™
 */

#ifndef INC_BURST_H_
#define INC_BURST_H_

#include <stdint.h>

typedef struct _PulseTrain PulseTrain;                    // Opaque type.
typedef uint8_t burst_size_t;
typedef uint8_t burst_phase_t;

// Class methods.
burst_size_t PulseTrain_size();
PulseTrain  *PulseTrain_new(void *addr, burst_size_t);
PulseTrain  *PulseTrain_copy(void *addr, burst_size_t, PulseTrain const *original);

// Instance methods.
PulseTrain   *PulseTrain_init(PulseTrain *, uint8_t electrode_set[2], uint16_t nr_of_pulses);
void          PulseTrain_setPhase(PulseTrain *, burst_phase_t phase);
burst_phase_t PulseTrain_phase(PulseTrain const *);
PulseTrain   *PulseTrain_setStartTimeMicros(PulseTrain *, uint32_t start_time_micros);
uint16_t      PulseTrain_nrOfPulses(PulseTrain const *);
void          PulseTrain_print(PulseTrain const *);

#endif
