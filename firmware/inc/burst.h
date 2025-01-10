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

#include <stdbool.h>
#include <stdint.h>

#define MIN_PULSE_WIDTH_¼_µs      6
#define MAX_PULSE_WIDTH_¼_µs    800

#define MIN_PULSE_PACE_µs      5000
#define MAX_PULSE_PACE_µs     63000

typedef struct {
    uint8_t  elcon[2];
    uint16_t pace_µs;
    uint16_t nr_of_pulses;
    uint16_t pulse_width_¼_µs;
    uint8_t  phase;
} Burst;

typedef struct {
    int8_t delta_width_¼_µs;                    // [0.25 µs]. Changes the duration of a pulse.
    int8_t delta_pace_µs;                       // [µs]. Modifies the time between pulses.
} Deltas;


bool Burst_isValid(Burst const *);
uint32_t Burst_duration_µs(Burst const *);
void Burst_applyDeltas(Burst *, Deltas const *);

#endif
