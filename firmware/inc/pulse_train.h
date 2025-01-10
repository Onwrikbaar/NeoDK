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

// Class method.
uint16_t PulseTrain_size();

// Instance methods.
PulseTrain *PulseTrain_init(PulseTrain *, uint8_t seq_nr, uint32_t timestamp, Burst const *burst);
bool     PulseTrain_isValid(PulseTrain const*, uint16_t sz);
void     PulseTrain_clearDeltas(PulseTrain *);
void     PulseTrain_setDeltas(PulseTrain *, int8_t delta_width_¼_µs, int8_t delta_pace_µs);
uint16_t PulseTrain_amplitude(PulseTrain const *);
uint8_t  PulseTrain_pulseWidth(PulseTrain const *);
Burst  const *PulseTrain_getBurst(PulseTrain const *, Burst *);
Deltas const *PulseTrain_getDeltas(PulseTrain const *, uint16_t sz, Deltas *);
void     PulseTrain_print(PulseTrain const *, uint16_t sz);

#endif
