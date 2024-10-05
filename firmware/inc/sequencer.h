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

#define DEFAULT_PRIMARY_VOLTAGE_mV      1500

typedef struct _Sequencer Sequencer;            // Opaque type.


// Class method.
Sequencer *Sequencer_new(void);

// Instance methods.
void Sequencer_start(Sequencer *);
bool Sequencer_handleEvent(Sequencer *);
void Sequencer_stop(Sequencer *);
void Sequencer_delete(Sequencer *);

#endif
