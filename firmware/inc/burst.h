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

#define MIN_PULSE_WIDTH_¼µs       6
#define MAX_PULSE_WIDTH_¼µs     800

#define MIN_PULSE_PACE_µs      5000
#define MAX_PULSE_PACE_µs     62500             // 16 Hz.

typedef struct {
    uint32_t start_time_µs;
    uint8_t  elcon[2];
    uint16_t pace_µs;
    uint16_t nr_of_pulses;
    uint16_t pulse_width_¼µs;
    uint8_t  phase;
    uint8_t  amplitude;
    uint8_t  flags;
} Burst;

typedef struct {
    int8_t delta_width_¼µs;                     // [0.25 µs]. Changes the duration of a pulse.
    int8_t delta_pace_µs;                       // [µs]. Modifies the time between pulses.
} Deltas;


#ifdef __cplusplus
extern "C" {
#endif

bool Burst_isValid(Burst const *);
uint32_t Burst_duration_µs(Burst const *);
uint8_t Burst_pulseWidth_µs(Burst const *);
Burst *Burst_adjust(Burst *);
void Burst_applyDeltas(Burst *, Deltas const *);
void Burst_print(Burst const *);

#ifdef __cplusplus
}
#endif

#endif
