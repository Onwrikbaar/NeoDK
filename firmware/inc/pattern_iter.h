/*
 * pattern_iter.h
 *
 *  NOTICE (do not remove):
 *      This file is part of project NeoDK (https://github.com/Onwrikbaar/NeoDK).
 *      See https://github.com/Onwrikbaar/NeoDK/blob/main/LICENSE.txt for full license details.
 *
 *  Created on: 6 Mar 2024
 *      Author: mark
 *   Copyright  2024 Neostim™
 */

#ifndef INC_PATTERN_ITER_H_
#define INC_PATTERN_ITER_H_

#include "bsp_app.h"

typedef struct {
    char const *name;
    uint8_t const (*pattern)[2];
    uint16_t nr_of_elcons;
    uint8_t pace_ms;
    uint8_t nr_of_steps;
    uint16_t nr_of_reps;
} PatternDescr;

typedef struct {
    uint8_t const (*pattern)[2];
    uint16_t nr_of_elcons, elcon_nr;
    uint8_t nr_of_steps, step_nr;
    uint8_t segment_nr;
    uint8_t pulse_width_micros;
    uint8_t pace_ms;
    uint16_t nr_of_reps;
} PatternIterator;


// Class method.
bool PatternIterator_checkPattern(uint8_t const pattern[][2], uint16_t nr_of_elcons);

// Instance methods.
void PatternIterator_init(PatternIterator *, PatternDescr const *);
bool PatternIterator_done(PatternIterator *);
bool PatternIterator_getNextPulseTrain(PatternIterator *, PulseTrain *);

#endif
