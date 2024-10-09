/*
 * controller.h
 *
 *  NOTICE (do not remove):
 *      This file is part of project NeoDK (https://github.com/Onwrikbaar/NeoDK).
 *      See https://github.com/Onwrikbaar/NeoDK/blob/main/LICENSE.txt for full license details.
 *
 *  Created on: 21 Aug 2024
 *      Author: mark
 *   Copyright  2024 Neostim™
 */

#ifndef INC_CONTROLLER_H_
#define INC_CONTROLLER_H_

#include "sequencer.h"
#include "datalink.h"

typedef struct _Controller Controller;          // Opaque type.

// Class method.
Controller *Controller_new();

// Instance methods.
void Controller_init(Controller *, Sequencer *, DataLink *);
void Controller_start(Controller *);
bool Controller_handleEvent(Controller *);
void Controller_stop(Controller *);
void Controller_delete(Controller *);

#endif
