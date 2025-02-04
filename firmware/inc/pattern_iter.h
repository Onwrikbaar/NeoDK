/*
 * pattern_iter.h
 *
 *  NOTICE (do not remove):
 *      This file is part of project NeoDK (https://github.com/Onwrikbaar/NeoDK).
 *      See https://github.com/Onwrikbaar/NeoDK/blob/main/LICENSE.txt for full license details.
 *
 *  Created on: 6 Mar 2024
 *      Author: mark
 *   Copyright  2024, 2025 Neostim™
 */

#ifndef INC_PATTERN_ITER_H_
#define INC_PATTERN_ITER_H_

#include "bsp_app.h"
#include "patterns.h"

typedef struct {
    PatternDescr const *pattern_descr;
    uint16_t elcon_nr;
    uint8_t pulse_width_micros;
    uint8_t amplitude;
    uint8_t step_nr;
    uint16_t nr_of_reps;
    uint8_t segment_nr;
} PatternIterator;


bool PatternIterator_init(PatternIterator *, PatternDescr const *);
void PatternIterator_setPulseWidth(PatternIterator *, uint8_t width_µs);
void PatternIterator_setIntensity(PatternIterator *, uint8_t intensity);
bool PatternIterator_scheduleNextBurst(PatternIterator *);
char const *PatternIterator_name(PatternIterator const *);
bool PatternIterator_done(PatternIterator *);

#endif
