/*
 * ao_event.h -- the Event class
 * 
 *  Created on: 11 Apr 2021
 *      Author: mark
 *   Copyright  2021..2024 Neostim™
 */

#ifndef INC_AO_EVENT_H_
#define INC_AO_EVENT_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _AOEvent AOEvent;                // Opaque type.
typedef uint16_t EventSize;
typedef enum {
    // These framework events MUST be the first five, and must remain in this order.
    ET_AO_NONE, ET_AO_ENTRY, ET_AO_EXIT, ET_AO_COLLECT_EXITS, ET_AO_COLLECT_ENTRIES,
    // Some common system events.
    ET_SPURIOUS_INTERRUPT, ET_LOG_AO_STATE, ET_POSIX_SIGNAL, ET_CONSOLE_INPUT,
    ET_COMMS_CAN_ACCEPT_DATA, ET_COMMS_HAS_DATA_AVAILABLE, ET_COMMS_ERROR,
    ET_BUTTON_PUSHED, ET_BUTTON_HELD, ET_BUTTON_RELEASED,
    ET_CHARGING_STARTED, ET_CHARGING_STOPPED,
    ET_LOG_FROM_IRQ, ET_APP_TIMER_EXPIRED,
    // Application events start here.
    ET_AO_FIRST_APP_EVENT
} EventType;

// Class methods.
EventSize AOEvent_minimumSize();
AOEvent  *AOEvent_newEntryEvent();
AOEvent  *AOEvent_newExitEvent();

// Instance methods.
AOEvent  *AOEvent_init(AOEvent *, EventType event_type, EventSize data_size);
EventSize AOEvent_size(AOEvent const *);
uint32_t  AOEvent_timeStampMicros(AOEvent const *);
uint32_t  AOEvent_ageMicros(AOEvent const *);
uint8_t   AOEvent_type(AOEvent const *);
EventSize AOEvent_dataSize(AOEvent const *);
uint8_t const *AOEvent_data(AOEvent const *);

#ifdef __cplusplus
}
#endif

#endif
