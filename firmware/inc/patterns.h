/*
 * patterns.h
 *
 *  NOTICE (do not remove):
 *      This file is part of project NeoDK (https://github.com/Onwrikbaar/NeoDK).
 *      See https://github.com/Onwrikbaar/NeoDK/blob/main/LICENSE.txt for full license details.
 *
 *  Created on: 11 Jan 2025
 *      Author: mark
 *   Copyright  2025 Neostim™
 */

#include <stdint.h>

typedef struct _PatternDescr PatternDescr;

struct _PatternDescr {
    char const *name;
    uint8_t const (*pattern)[2];
    uint16_t nr_of_elcons;
    uint16_t pace_µs;
    uint16_t nr_of_reps;
    uint8_t nr_of_steps;
};


uint16_t Patterns_getCount();
char const *Patterns_name(PatternDescr const *);
void Patterns_getNames(char const *[], uint8_t cnt);
void Patterns_checkAll();
PatternDescr const *Patterns_findByName(char const *, uint16_t len);
PatternDescr const *Patterns_getNext(PatternDescr const *);
