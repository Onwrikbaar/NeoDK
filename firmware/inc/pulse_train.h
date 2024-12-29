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

#ifndef INC_PULSE_TRAIN_H_
#define INC_PULSE_TRAIN_H_

#include "burst.h"

typedef struct _PulseTrain PulseTrain;          // Opaque type.
typedef uint8_t pulse_train_size_t;

// Class methods.
pulse_train_size_t PulseTrain_size();
PulseTrain *PulseTrain_new(void *addr, pulse_train_size_t);
PulseTrain *PulseTrain_copy(void *addr, pulse_train_size_t, PulseTrain const *original);

// Instance methods.
PulseTrain *PulseTrain_init(PulseTrain *, uint8_t electrode_set[2], uint16_t nr_of_pulses);
PulseTrain *PulseTrain_setStartTimeMicros(PulseTrain *, uint32_t start_time_micros);
uint16_t    PulseTrain_amplitude(PulseTrain const *);
uint8_t     PulseTrain_pulseWidth(PulseTrain const *);
Burst const *PulseTrain_getBurst(PulseTrain const *, Burst *);
void        PulseTrain_print(PulseTrain const *);

#endif
