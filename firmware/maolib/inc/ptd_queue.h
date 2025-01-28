/*
 * ptd_queue.h
 *
 *  NOTICE (do not remove):
 *      This file is part of project NeoDK (https://github.com/Onwrikbaar/NeoDK).
 *      See https://github.com/Onwrikbaar/NeoDK/blob/main/LICENSE.txt for full license details.
 *
 *  Created on: 12 Jan 2025
 *      Author: mark
 *   Copyright  2025 Neostim™
 */

#ifndef INC_PTD_QUEUE_H_
#define INC_PTD_QUEUE_H_

#include <stdbool.h>
#include <stdint.h>

#include "pulse_train.h"

typedef struct _PtdQueue PtdQueue;              // Opaque type.

#ifdef __cplusplus
extern "C" {
#endif

// Class method.
PtdQueue *PtdQueue_new(uint16_t nr_of_descriptors);

// Instance methods.
void PtdQueue_clear(PtdQueue *);
bool PtdQueue_isEmpty(PtdQueue const *);
uint16_t PtdQueue_nrOfBytesFree(PtdQueue const *, uint16_t[2]);
bool PtdQueue_addDescriptor(PtdQueue *, PulseTrain const *, uint16_t sz, uint8_t *err);
bool PtdQueue_getNextBurst(PtdQueue *, Burst *);
void PtdQueue_delete(PtdQueue *);

#ifdef __cplusplus
}
#endif

#endif
