/*
 * controller.c -- the interface to the controlling client
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

#include "bsp_dbg.h"
#include "bsp_app.h"
#include "matter.h"
#include "attributes.h"
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

typedef struct {
    uint16_t transaction_id;
    uint8_t  opcode;
    uint8_t  reserved;
    uint16_t attribute_id;
    uint8_t  data[0];
} AttributeAction;

typedef void *(*StateFunc)(Controller *, AOEvent const *);

struct _Controller {
    EventQueue event_queue;                     // This MUST be the first member.
    uint8_t    event_storage[400];
    StateFunc  state;
    Sequencer *sequencer;
    DataLink  *datalink;
};


static uint8_t const welcome_msg[] = "Push the button to play or pause :-)\n";


static void initResponsePacket(PacketHeader *ph)
{
    ph->flags = 0x00;
    ph->dst_address = ph->src_address;
    ph->src_address = 0x0000;                   // TODO Use our real address.
    ph->reserved = 0x00;
}


static void initAttributeAction(AttributeAction *rsp_aa, AttributeAction const *req_aa)
{
    *rsp_aa = *req_aa;                          // Copy the request.
    rsp_aa->opcode = OC_REPORT_DATA;            // Only change the opcode.
}


static void readPatternNames(Controller *me, AttributeAction const *aa)
{
    uint8_t nr_of_patterns = Sequencer_getNrOfPatterns(me->sequencer);
    char const *pattern_names[nr_of_patterns];  // Reserve enough space.
    Sequencer_getPatternNames(me->sequencer, pattern_names, nr_of_patterns);
    uint16_t nbtw = sizeof(PacketHeader) + sizeof(AttributeAction);
    uint16_t packet_size = nbtw + Matter_encodedStringArrayLength(pattern_names, nr_of_patterns);
    // TODO Ensure packet_size does not exceed max frame payload size.
    uint8_t packet[packet_size];
    initResponsePacket((PacketHeader *)packet);
    initAttributeAction((AttributeAction *)(packet + sizeof(PacketHeader)), aa);
    nbtw += Matter_encodeStringArray(packet + nbtw, pattern_names, nr_of_patterns);
    DataLink_sendDatagram(me->datalink, packet, nbtw);
}


static void attributeChanged(Controller *me, AttributeId ai, ElementEncoding enc, uint8_t const *data, uint16_t data_size)
{
    // BSP_logf("Controller_%s(%hu) size=%hu\n", __func__, ai, data_size);
    uint16_t nbtw = sizeof(PacketHeader) + sizeof(AttributeAction);
    uint16_t packet_size = nbtw + Matter_encodedDataLength(enc, data_size);
    uint8_t packet[packet_size];
    initResponsePacket((PacketHeader *)packet);
    AttributeAction *aa = (AttributeAction *)(packet + sizeof(PacketHeader));
    aa->transaction_id = 0;
    aa->opcode = OC_REPORT_DATA;
    aa->reserved = 0;
    aa->attribute_id = ai;
    // TODO Add subscription Id?
    nbtw += Matter_encodeScalarData(packet + nbtw, enc, data, data_size);
    DataLink_sendDatagram(me->datalink, packet, nbtw);
}


static void handleReadRequest(Controller *me, AttributeAction const *aa)
{
    switch (aa->attribute_id)
    {
        case AI_ALL_PATTERN_NAMES:
            readPatternNames(me, aa);
            break;
        case AI_CURRENT_PATTERN_NAME:
            Sequencer_notifyPattern(me->sequencer);
            break;
        case AI_INTENSITY_PERCENT:
            Sequencer_notifyIntensity(me->sequencer);
            break;
        case AI_PLAY_PAUSE_STOP:
            Sequencer_notifyPlayState(me->sequencer);
            break;
        default:
            BSP_logf("%s: unknown attribute id=%hu\n", __func__, aa->attribute_id);
            // TODO Respond with NOT_FOUND code.
    }
}


static EventType eventTypeForCommand(uint8_t const *cs, uint16_t len)
{
    if (len == 4 && memcmp(cs, "play",  len) == 0) return ET_PLAY;
    if (len == 5 && memcmp(cs, "pause", len) == 0) return ET_PAUSE;
    if (len == 4 && memcmp(cs, "stop",  len) == 0) return ET_STOP;
    return ET_UNKNOWN_COMMAND;
}


static void handleWriteRequest(Controller *me, AttributeAction const *aa)
{
    switch (aa->attribute_id)
    {
        case AI_INTENSITY_PERCENT:
            if (aa->data[0] == EE_UNSIGNED_INT_1) {
                EventQueue_postEvent((EventQueue *)me->sequencer, ET_SET_INTENSITY, &aa->data[1], sizeof aa->data[1]);
            }
            break;
        case AI_CURRENT_PATTERN_NAME:
            if (aa->data[0] == EE_UTF8_1LEN) {
                EventQueue_postEvent((EventQueue *)me->sequencer, ET_SELECT_PATTERN_BY_NAME, aa->data + 2, aa->data[1]);
            }
            break;
        case AI_PLAY_PAUSE_STOP:
            if (aa->data[0] == EE_UTF8_1LEN) {
                EventQueue_postEvent((EventQueue *)me->sequencer, eventTypeForCommand(aa->data + 2, aa->data[1]), NULL, 0);
            }
            break;
        default:
            BSP_logf("%s: unknown attribute id=%hu\n", __func__, aa->attribute_id);
            // TODO Respond with NOT_FOUND code.
    }
}


static void handleSubscribeRequest(Controller *me, AttributeAction const *aa)
{
    Attribute_subscribe(aa->attribute_id, (AttrNotifier)&attributeChanged, me);
}


static void handleInvokeRequest(Controller *me, AttributeAction const *aa)
{
    switch (aa->attribute_id)
    {
        case AI_CURRENT_PATTERN_NAME:
            if (aa->data[0] == EE_UTF8_1LEN) {
                EventQueue_postEvent((EventQueue *)me->sequencer, ET_SELECT_PATTERN_BY_NAME, aa->data + 2, aa->data[1]);
            }
            break;
        default:
            BSP_logf("%s: unknown attribute id=%hu\n", __func__, aa->attribute_id);
            // TODO Respond with NOT_FOUND code.
    }
}


static void handleRequest(Controller *me, AttributeAction const *aa)
{
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
        case OC_SUBSCRIBE_REQUEST:
            BSP_logf("Transaction %hu: subscribe to attribute %hu\n", aa->transaction_id, aa->attribute_id);
            handleSubscribeRequest(me, aa);
            handleReadRequest(me, aa);
            break;
        case OC_INVOKE_REQUEST:
            BSP_logf("Transaction %hu: invoke %hu\n", aa->transaction_id, aa->attribute_id);
            handleInvokeRequest(me, aa);
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
            handleRequest(me, (AttributeAction const *)(AOEvent_data(evt) + sizeof(PacketHeader)));
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
}


void Controller_start(Controller *me)
{
    me->state = &stateIdle;
    me->state(me, AOEvent_newEntryEvent());
    DataLink_open(me->datalink, &me->event_queue);
    DataLink_waitForSync(me->datalink);
    BSP_logf("Starting NeoDK!\n%s", welcome_msg);
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
