/*
 * patterns.c
 *
 *  NOTICE (do not remove):
 *      This file is part of project NeoDK (https://github.com/Onwrikbaar/NeoDK).
 *      See https://github.com/Onwrikbaar/NeoDK/blob/main/LICENSE.txt for full license details.
 *
 *  Created on: 11 Jan 2025
 *      Author: mark
 *   Copyright  2025 Neostim™
 */

#include <string.h>

#include "bsp_dbg.h"
#include "convenience.h"
#include "burst.h"

// This module implements:
#include "patterns.h"

enum { EL_0, EL_A, EL_B, EL_C = 4, EL_AC = (EL_A | EL_C), EL_D = 8, EL_BD = (EL_B | EL_D) };

__attribute__((unused))
static char const elset_str[][5] = {
    "None", "A", "B", "AB", "C", "AC", "BC", "ABC", "D", "AD", "BD", "ABD", "CD", "ACD", "BCD", "ABCD"
};

static uint8_t const pattern_jackhammer[][2] =
{
    {EL_AC, EL_BD},
    {EL_BD, EL_AC},
};

static uint8_t const pattern_toggle[][2] =
{
    {EL_A, EL_B},
    {EL_A, EL_B},
    {EL_C, EL_D},
    {EL_B, EL_A},
    {EL_B, EL_A},
    {EL_D, EL_C},
};

static uint8_t const pattern_cross_toggle[][2] =
{
    {EL_A, EL_D},
    {EL_B, EL_C},
    {EL_C, EL_B},
    {EL_D, EL_A},
};

static uint8_t const pattern_circle[][2] =
{
    {EL_A,  EL_B},
    {EL_B,  EL_AC},
    {EL_C,  EL_B},
    {EL_BD, EL_C},
    {EL_C,  EL_D},
    {EL_D,  EL_AC},
    {EL_A,  EL_D},
    {EL_BD, EL_A},
    {EL_AC, EL_BD},

    {EL_B,  EL_A},
    {EL_AC, EL_B},
    {EL_B,  EL_C},
    {EL_C,  EL_BD},
    {EL_D,  EL_C},
    {EL_AC, EL_D},
    {EL_D,  EL_A},
    {EL_A,  EL_BD},
    {EL_BD, EL_AC},
};

static uint8_t const pattern_itch[][2] =
{
    {EL_A, EL_B},
    {EL_B, EL_A},
    {EL_A, EL_B},
    {EL_B, EL_A},

    // {EL_AC, EL_BD},

    {EL_C, EL_D},
    {EL_D, EL_C},
    {EL_C, EL_D},
    {EL_D, EL_C},

    // {EL_BD, EL_AC},

    {EL_A, EL_B},
    {EL_C, EL_D},
    {EL_B, EL_A},
    {EL_D, EL_C},
};

static PatternDescr const pattern_descriptors[] =
{
    {
        .name = "Jackhammer",
        .pattern = pattern_jackhammer,
        .nr_of_elcons = M_DIM(pattern_jackhammer),
        .pace_µs = MAX_PULSE_PACE_µs,
        .nr_of_steps = 3,
        .nr_of_reps = 200,
    },
    {
        .name = "Toggle",
        .pattern = pattern_toggle,
        .nr_of_elcons = M_DIM(pattern_toggle),
        .pace_µs = 25000,
        .nr_of_steps = 5,
        .nr_of_reps = 300,
    },
    {
        .name = "CrossToggle",
        .pattern = pattern_cross_toggle,
        .nr_of_elcons = M_DIM(pattern_cross_toggle),
        .pace_µs = 20000,
        .nr_of_steps = 5,
        .nr_of_reps = 200,
    },
    {
        .name = "Circle",
        .pattern = pattern_circle,
        .nr_of_elcons = M_DIM(pattern_circle),
        .pace_µs = 30000,
        .nr_of_steps = 9,
        .nr_of_reps = 40,
    },
    {
        .name = "Scratch that itch",
        .pattern = pattern_itch,
        .nr_of_elcons = M_DIM(pattern_itch),
        .pace_µs = 8000,
        .nr_of_steps = 11,
        .nr_of_reps = 5000,
    },
};


static bool checkPattern(uint8_t const pattern[][2], uint16_t nr_of_elcons)
{
    for (uint16_t i = 0; i < nr_of_elcons; i++) {
        uint8_t const *elcon = pattern[i];
        if ((elcon[0] & elcon[1]) != 0) {
            BSP_logf("%s: short in elcon %hu\n", __func__, i);
            return false;
        }
    }
    return true;
}

/*
 * Below are the functions implementing this module's interface.
 */

uint16_t Patterns_getCount()
{
    return M_DIM(pattern_descriptors);
}


char const *Patterns_name(PatternDescr const *pd)
{
    M_ASSERT(pd != NULL);
    return pd->name;
}


void Patterns_getNames(char const *names[], uint8_t cnt)
{
    if (cnt > M_DIM(pattern_descriptors)) cnt = M_DIM(pattern_descriptors);
    for (uint8_t i = 0; i < cnt; i++) {
        names[i] = pattern_descriptors[i].name;
    }
}


void Patterns_checkAll()
{
    for (uint8_t i = 0; i < M_DIM(pattern_descriptors); i++) {
        PatternDescr const *pd = &pattern_descriptors[i];
        BSP_logf("Checking '%s'\n", pd->name);
        checkPattern(pd->pattern, pd->nr_of_elcons);
    }
}


PatternDescr const *Patterns_findByName(char const *name, uint16_t len)
{
    for (uint16_t i = 0; i < M_DIM(pattern_descriptors); i++) {
        PatternDescr const *pd = &pattern_descriptors[i];
        if (len == strlen(pd->name) && memcmp(name, pd->name, len) == 0) {
            return pd;
        }
    }
    return NULL;
}


PatternDescr const *Patterns_getNext(PatternDescr const *pd)
{
    uint16_t index = 0;
    if (pd != NULL) {
        index = pd - pattern_descriptors;
        if (++index == M_DIM(pattern_descriptors)) index = 0;
    }
    return &pattern_descriptors[index];
}
