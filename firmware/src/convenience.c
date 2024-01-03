/*
 * convenience.c
 *
 *  Created on: 14 Oct 2020
 *      Author: mark
 *   Copyright  2020..2024 Neostim
 */

#include <stdbool.h>
#include <string.h>
#include <stdio.h>

// This module implements:
#include "convenience.h"


Selector *Selector_init(Selector *me, Action action, void *target)
{
    me->nr_of_times_invoked = 0;
    me->target = target;
    me->action = action;
    return me;
}


void invokeSelector(Selector *sel, uint32_t val)
{
    if (sel->action) {
        sel->action(sel->target, val);
        sel->nr_of_times_invoked += 1;
    }
}

// TODO Make sure this handles negative increments correctly too.
struct timespec *tsIncrementNanos(struct timespec *tsp, int64_t nanoseconds)
{
    int32_t const one_billion = 1000000000L;
    nanoseconds += tsp->tv_nsec;
    time_t const extra_seconds = nanoseconds / one_billion;
    tsp->tv_sec += extra_seconds;
    tsp->tv_nsec = nanoseconds - (int64_t)extra_seconds * one_billion;
    return tsp;
}


char const *basenameOfPath(char const *path)
{
    char const *last_slash_pos = strrchr(path, '/');
    return last_slash_pos ? last_slash_pos + 1 : path;
}


char const *bytesToHexString(uint8_t const *pb, uint16_t nb)
{
    #define MAX_NB   32
    static char hs[MAX_NB * 3 + 1];             // Note: this gets overwritten with each call.

    if (nb > MAX_NB) nb = MAX_NB;
    char *p = hs;
    for (uint8_t i = 0; i < nb; i++) {
        snprintf(p, 4, " %02x", pb[i]);
        p += 3;
    }
    return hs;
}
