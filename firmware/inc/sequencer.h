/*
 * sequencer.h
 *
 *  Created on: 27 Feb 2024
 *      Author: mark
 *   Copyright  2024 Neostim
 */

#ifndef INC_SEQUENCER_H_
#define INC_SEQUENCER_H_

#include <stdint.h>

typedef struct _Sequencer Sequencer;            // Opaque type.


// Class method.
Sequencer *Sequencer_new();

// Instance methods.
void Sequencer_startPulseTrain(Sequencer *, uint8_t phase);
void Sequencer_delete(Sequencer *);

#endif
