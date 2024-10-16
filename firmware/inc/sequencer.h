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

typedef struct _Sequencer Sequencer;            // Opaque type.

typedef enum { PS_UNKNOWN, PS_IDLE, PS_PAUSED, PS_PLAYING } PlayState;

// Class method.
Sequencer *Sequencer_new(void);

// Instance methods.
Sequencer *Sequencer_init(Sequencer *);
void Sequencer_start(Sequencer *);
bool Sequencer_handleEvent(Sequencer *);

uint16_t Sequencer_getNrOfPatterns(Sequencer const *);
void Sequencer_getPatternNames(Sequencer const *, char const *[], uint8_t);
uint8_t Sequencer_getIntensityPercentage(Sequencer const *);

void Sequencer_notifyIntensity(Sequencer const *);
void Sequencer_notifyPattern(Sequencer const *);
void Sequencer_notifyPlayState(Sequencer const *);

void Sequencer_stop(Sequencer *);
void Sequencer_delete(Sequencer *);

#endif
