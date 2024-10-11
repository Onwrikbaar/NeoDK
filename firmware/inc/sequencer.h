/*
 * sequencer.h
 *
 *  NOTICE (do not remove):
 *      This file is part of project NeoDK (https://github.com/Onwrikbaar/NeoDK).
 *      See https://github.com/Onwrikbaar/NeoDK/blob/main/LICENSE.txt for full license details.
 *
 *  Created on: 27 Feb 2024
 *      Author: mark
 *   Copyright  2024 Neostim™
 */

#ifndef INC_SEQUENCER_H_
#define INC_SEQUENCER_H_

#include <stdint.h>

#define DEFAULT_PRIMARY_VOLTAGE_mV      1200

typedef struct _Sequencer Sequencer;            // Opaque type.


// Class method.
Sequencer *Sequencer_new(void);

// Instance methods.
Sequencer *Sequencer_init(Sequencer *);
void Sequencer_start(Sequencer *);
bool Sequencer_handleEvent(Sequencer *);
uint16_t Sequencer_getNrOfPatterns(Sequencer const *me);
void Sequencer_getPatternNames(Sequencer const *, char const *[], uint8_t);
char const *Sequencer_getCurrentPatternName(Sequencer const *);
uint8_t Sequencer_getIntensityPercentage(Sequencer const *);
void Sequencer_stop(Sequencer *);
void Sequencer_delete(Sequencer *);

#endif
