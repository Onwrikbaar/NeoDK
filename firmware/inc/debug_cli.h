/*
 * debug_cli.h
 *
 *  NOTICE (do not remove):
 *      This file is part of project NeoDK (https://github.com/Onwrikbaar/NeoDK).
 *      See https://github.com/Onwrikbaar/NeoDK/blob/main/LICENSE.txt for full license details.
 *
 *  Created on: 26 Mar 2024
 *      Author: mark
 *   Copyright  2024 Neostim™
 */

#ifndef INC_DEBUG_CLI_H_
#define INC_DEBUG_CLI_H_

#include "eventqueue.h"
#include "sequencer.h"
#include "datalink.h"


void CLI_init(EventQueue *, Sequencer *, DataLink *);
int  CLI_logf(char const *fmt, ...);
void CLI_handleRemoteInput(uint8_t const *, uint16_t nb);

#endif
