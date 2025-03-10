/*
 * convenience.h
 *
 *  Created on: 14 Oct 2020
 *      Author: mark
 *   Copyright  2020..2025 Neostim™
 */

#ifndef INC_CONVENIENCE_H_
#define INC_CONVENIENCE_H_

#ifndef _POSIX_MONOTONIC_CLOCK
#define _POSIX_MONOTONIC_CLOCK      (-1)
#endif
#include <stdbool.h>
#include <time.h>
#include <stdint.h>

#include "eventqueue.h"

#define M_DIM(arr)      (sizeof arr / sizeof arr[0])

typedef int8_t DeviceId;
#define UNDEFINED_DEVICE_ID     ((DeviceId)(-1))

typedef int (*CompareFunc)(const void *, const void *);
typedef void (*Action)(void *target, uint32_t);
typedef struct {
    char const *fmt;
    struct { uint32_t argv[4]; } as;
} LogArgs;

typedef struct {
    Action action;
    void *target;
    uint32_t nr_of_times_invoked;
} Selector;

#ifdef __cplusplus
extern "C" {
#endif

Selector *Selector_init(Selector *, Action, void *target);
void invokeSelector(Selector *, uint32_t);
int dumpBuffer(const char *prefix, const uint8_t *bbuf, uint8_t nb);
struct timespec *tsIncrementNanos(struct timespec *, int64_t nanoseconds);
char const *bytesToHexString(uint8_t const *pb, uint16_t nb);
void DBG_irqLogf(EventQueue *, char const *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif
