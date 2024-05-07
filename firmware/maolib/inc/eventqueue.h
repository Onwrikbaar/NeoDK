/*
 * eventqueue.h -- the Event Queue class
 * 
 *  Created on: 11 Apr 2021
 *      Author: mark
 *   Copyright  2021..2024 Neostim
 */

#ifndef INC_EVENTQUEUE_H_
#define INC_EVENTQUEUE_H_

#include <stdbool.h>

#include "circbuffer.h"
#include "ao_event.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    CircBuffer buffer;
} EventQueue;

typedef void (*EvtFunc)(void *, AOEvent const *);

// Instance methods.
void EventQueue_init(EventQueue *, uint8_t *storage, uint16_t size);
void EventQueue_clear(EventQueue *);
bool EventQueue_isEmpty(EventQueue const *);
uint32_t EventQueue_availableSpace(EventQueue const *);
bool EventQueue_postEvent(EventQueue *, uint8_t event_type, uint8_t const *data, EventSize len);
bool EventQueue_repostEvent(EventQueue *, AOEvent const *);
bool EventQueue_handleNextEvent(EventQueue *, EvtFunc, void *target);
void EventQueue_forAll(EventQueue *, EvtFunc, void *target);

#ifdef __cplusplus
}
#endif

#endif
