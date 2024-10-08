/*
 * controller.c
 *
 *  NOTICE (do not remove):
 *      This file is part of project NeoDK (https://github.com/Onwrikbaar/NeoDK).
 *      See https://github.com/Onwrikbaar/NeoDK/blob/main/LICENSE.txt for full license details.
 *
 *  Created on: 21 Aug 2024
 *      Author: mark
 *   Copyright  2024 Neostim™
 */

#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#include "bsp_dbg.h"
#include "bsp_app.h"
#include "app_event.h"

// This module implements:
#include "controller.h"

typedef struct {
    uint8_t  flags;                             // Version, etc.
    uint8_t  reserved;                          // Hop count, etc.
    uint16_t src_address;
    uint16_t dst_address;
    uint8_t  message[0];
} PacketHeader;

typedef enum {
    OC_NONE, OC_STATUS_RESPONSE, OC_READ_REQUEST, OC_SUBSCRIBE_REQUEST, OC_SUBSCRIBE_RESPONSE, OC_REPORT_DATA,
    OC_WRITE_REQUEST, OC_WRITE_RESPONSE, OC_INVOKE_REQUEST, OC_INVOKE_RESPONSE, OC_TIMED_REQUEST
} Opcode;

typedef enum {
    EE_SIGNED_INT_1, EE_SIGNED_INT_2, EE_SIGNED_INT_4, EE_SIGNED_INT_8,
    EE_UNSIGNED_INT_1, EE_UNSIGNED_INT_2, EE_UNSIGNED_INT_4, EE_UNSIGNED_INT_8,
    EE_BOOLEAN_FALSE, EE_BOOLEAN_TRUE, EE_FLOAT_4, EE_FLOAT_8,
    EE_UTF8_1LEN, EE_UTF8_2LEN, EE_UTF8_4LEN, EE_UTF8_8LEN,
    EE_BYTE_1LEN, EE_BYTE_2LEN, EE_BYTE_4LEN, EE_BYTE_8LEN,
    EE_NULL, EE_STRUCT, EE_ARRAY, EE_LIST, EE_END_OF_CONTAINER
} ElementEncoding;

typedef enum {
    AI_PATTERN_NAMES = 5,
} AttributeId;

typedef struct {
    uint16_t transaction_id;
    uint8_t opcode;
    uint8_t reserved;
    uint16_t attribute_id;
    uint8_t data[0];
} AttributeAction;


typedef void *(*StateFunc)(Controller *, AOEvent const *);

struct _Controller {
    EventQueue event_queue;                     // This MUST be the first member.
    uint8_t event_storage[400];
    StateFunc state;
    Sequencer *sequencer;
    DataLink *datalink;
};


static uint8_t const welcome_msg[] = "Push the button to play or pause :-)\n";


static uint16_t encodeString(uint8_t dst[], char const *str)
{
    uint16_t nbe = 0;
    dst[nbe++] = EE_UTF8_1LEN;
    uint8_t str_len = strlen(str);
    dst[nbe++] = str_len;
    memcpy(dst + nbe, str, str_len);
    return nbe + str_len;
}


static uint16_t encodeStringArray(uint8_t dst[], char const *strings[], uint8_t nr_of_strings)
{
    uint16_t nbe = 0;
    dst[nbe++] = EE_ARRAY;
    for (uint8_t i = 0; i < nr_of_strings; i++) {
        nbe += encodeString(dst + nbe, strings[i]);
    }
    dst[nbe++] = EE_END_OF_CONTAINER;
    return nbe;
}

// Yeah, this function will be refactored :-)
static void readPatternNames(Controller *me, AttributeAction const *aa)
{
    uint8_t nr_of_patterns = Sequencer_nrOfPatterns(me->sequencer);
    char const *pattern_names[nr_of_patterns];
    Sequencer_getPatternNames(me->sequencer, pattern_names, nr_of_patterns);
    uint16_t total_bytes = 0;
    for (uint8_t i = 0; i < nr_of_patterns; i++) {
        total_bytes += strlen(pattern_names[i]);
    }
    uint16_t nbw = sizeof(PacketHeader) + sizeof(AttributeAction);
    uint16_t packet_size = nbw + 2 + nr_of_patterns * 2 + total_bytes;
    // TODO Check for max payload size.
    uint8_t packet[packet_size];
    PacketHeader *ph = (PacketHeader *)packet;
    // TODO Fill in the packet header.
    AttributeAction *rsp_aa = (AttributeAction *)ph->message;
    *rsp_aa = *aa;
    rsp_aa->opcode = OC_REPORT_DATA;
    nbw += encodeStringArray(packet + nbw, pattern_names, nr_of_patterns);
    DataLink_sendDatagram(me->datalink, packet, nbw);
}


static void handleReadRequest(Controller *me, AttributeAction const *aa)
{
    if (aa->attribute_id == AI_PATTERN_NAMES) {
        readPatternNames(me, aa);
    } else {
        BSP_logf("%s: unknown attribute id=%hu\n", __func__, aa->attribute_id);
        // TODO Respond with NOT_FOUND code.
    }
}


static void handleWriteRequest(Controller *me, AttributeAction const *aa)
{
    // TODO Implement.
}


static void handleRequest(Controller *me, uint8_t const *request)
{
    AttributeAction const *aa = (AttributeAction const *)request;
    switch (aa->opcode)
    {
        case OC_READ_REQUEST:
            BSP_logf("Transaction %hu: read attribute %hu\n", aa->transaction_id, aa->attribute_id);
            handleReadRequest(me, aa);
            break;
        case OC_WRITE_REQUEST:
            BSP_logf("Transaction %hu: write attribute %hu\n", aa->transaction_id, aa->attribute_id);
            handleWriteRequest(me, aa);
            break;
        default:
            BSP_logf("%s, unknown opcode 0x%02hhx\n", __func__, aa->opcode);
            break;
    }
}


static void *stateNop(Controller *me, AOEvent const *evt)
{
    BSP_logf("Controller_%s unexpected event: %u\n", __func__, AOEvent_type(evt));
    return NULL;
}


static void *stateIdle(Controller *me, AOEvent const *evt)
{
    switch (AOEvent_type(evt))
    {
        case ET_AO_ENTRY:
            BSP_logf("Controller_%s ENTRY\n", __func__);
            break;
        case ET_AO_EXIT:
            BSP_logf("Controller_%s EXIT\n", __func__);
            break;
        case ET_DEBUG_SYNC:
            DataLink_sendDebugPacket(me->datalink, welcome_msg, sizeof welcome_msg);
            break;
        case ET_INCOMING_PACKET:
            // Ignore the packet header for now.
            handleRequest(me, AOEvent_data(evt) + sizeof(PacketHeader));
            break;
        default:
            BSP_logf("Controller_%s unexpected event: %u\n", __func__, AOEvent_type(evt));
    }
    return NULL;
}

// Send one event to the state machine.
static void dispatchEvent(Controller *me, AOEvent const *evt)
{
    // BSP_logf("%s(%u)\n", __func__, AOEvent_type(evt));
    StateFunc new_state = me->state(me, evt);
    if (new_state != NULL) {                    // Transition.
        StateFunc sf = me->state(me, AOEvent_newExitEvent());
        // No transition allowed on ENTRY and EXIT events.
        M_ASSERT(sf == NULL);
        me->state = new_state;
        sf = me->state(me, AOEvent_newEntryEvent());
        M_ASSERT(sf == NULL);
    }
}

/*
 * Below are the functions implementing this module's interface.
 */

Controller *Controller_new()
{
    Controller *me = (Controller *)malloc(sizeof(Controller));
    EventQueue_init(&me->event_queue, me->event_storage, sizeof me->event_storage);
    me->state = &stateNop;
    return me;
}


void Controller_init(Controller *me, Sequencer *sequencer, DataLink *datalink)
{
    me->sequencer = sequencer;
    me->datalink  = datalink;
    BSP_logf("%s\n", __func__);
    me->state = &stateIdle;
    me->state(me, AOEvent_newEntryEvent());
    DataLink_open(me->datalink, &me->event_queue);
    DataLink_waitForSync(me->datalink);
}


void Controller_start(Controller *me)
{
    BSP_logf("Starting NeoDK!\n");
    BSP_logf("%s", welcome_msg);
    BSP_setPrimaryVoltage_mV(DEFAULT_PRIMARY_VOLTAGE_mV);
    BSP_primaryVoltageEnable(true);
}


bool Controller_handleEvent(Controller *me)
{
    return EventQueue_handleNextEvent(&me->event_queue, (EvtFunc)&dispatchEvent, me);
}


void Controller_stop(Controller *me)
{
    me->state(me, AOEvent_newExitEvent());
    me->state = stateNop;
    BSP_logf("End of session\n");
    DataLink_close(me->datalink);
}


void Controller_delete(Controller *me)
{
    free(me);
}
