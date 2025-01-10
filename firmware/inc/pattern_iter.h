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

typedef struct {
    char const *name;
    uint8_t const (*pattern)[2];
    uint16_t nr_of_elcons;
    uint16_t pace_µs;
    uint16_t nr_of_reps;
    uint8_t nr_of_steps;
} PatternDescr;

typedef struct {
    PatternDescr const *pattern_descr;
    uint16_t elcon_nr;
    uint8_t pulse_width_micros;
    uint8_t step_nr;
    uint16_t nr_of_reps;
    uint8_t segment_nr;
} PatternIterator;


// Class method.
bool PatternIterator_checkPattern(uint8_t const pattern[][2], uint16_t nr_of_elcons);

// Instance methods.
void PatternIterator_init(PatternIterator *, PatternDescr const *);
bool PatternIterator_done(PatternIterator *);
bool PatternIterator_getNextBurst(PatternIterator *, Burst *);

#endif
