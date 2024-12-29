/*
 * burst.h
 *
 *  NOTICE (do not remove):
 *      This file is part of project NeoDK (https://github.com/Onwrikbaar/NeoDK).
 *      See https://github.com/Onwrikbaar/NeoDK/blob/main/LICENSE.txt for full license details.
 *
 *  Created on: 29 Dec 2024
 *      Author: mark
 *   Copyright  2024, 2025 Neostim™
 */

#ifndef INC_BURST_H_
#define INC_BURST_H_

#include <stdint.h>

typedef struct {
    uint8_t elcon[2];
    uint8_t phase;
    uint8_t pace_ms;
    uint16_t nr_of_pulses;
    uint8_t pulse_width_micros;
} Burst;

#endif
