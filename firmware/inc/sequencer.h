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
Sequencer *Sequencer_new(void);

// Instance methods.
void Sequencer_start(Sequencer *);
void Sequencer_togglePlayPause(Sequencer *, uint32_t button_pushed);
void Sequencer_delete(Sequencer *);

#endif
