/*
 * attributes.h
 *
 *  NOTICE (do not remove):
 *      This file is part of project NeoDK (https://github.com/Onwrikbaar/NeoDK).
 *      See https://github.com/Onwrikbaar/NeoDK/blob/main/LICENSE.txt for full license details.
 *
 *  Created on: 15 Oct 2024
 *      Author: mark
 *   Copyright  2024 Neostim™
 */

#ifndef INC_ATTRIBUTES_H_
#define INC_ATTRIBUTES_H_

#include "matter.h"


typedef enum {
    AI_ALL_PATTERN_NAMES = 5, AI_CURRENT_PATTERN_NAME, AI_INTENSITY_PERCENT, AI_PLAY_PAUSE_STOP
} AttributeId;

typedef void (*AttrNotifier)(void *target, AttributeId, ElementEncoding, uint8_t const *data, uint16_t size);


bool Attribute_subscribe(AttributeId, ElementEncoding, AttrNotifier, void *target);
void Attribute_changed(AttributeId, uint8_t const *data, uint16_t size);

#endif
