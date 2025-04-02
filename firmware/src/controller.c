/*
 * controller.c -- the interface to the controlling client
 *
 *  NOTICE (do not remove):
 *      This file is part of project NeoDK (https://github.com/Onwrikbaar/NeoDK).
 *      See https://github.com/Onwrikbaar/NeoDK/blob/main/LICENSE.txt for full license details.
 *
 *  Created on: 21 Aug 2024
 *      Author: mark
 *   Copyright  2024, 2025 Neostim™
 */

#include <stdlib.h>
#include <string.h>

#include "bsp_dbg.h"
#include "bsp_app.h"
#include "matter.h"
#include "attributes.h"
#include "app_event.h"
#include "patterns.h"

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
    uint32_t   heartbeat_interval_µs;
    char       box_name[24];
};


static uint8_t const welcome_msg[] = "Push the button to play or pause :-)\n";


static void initResponsePacket(PacketHeader *ph)
{
    ph->flags = 0x00;
    ph->dst_address = ph->src_address;
    ph->src_address = 0x0000;                   // TODO Use our real address.
    ph->reserved = 0x00;
}


static void initAttributeAction(AttributeAction *aa, uint16_t transaction_id, uint8_t opcode, uint16_t attribute_id)
{
    aa->transaction_id = transaction_id;
    aa->opcode = opcode;
    aa->reserved = 0;
    aa->attribute_id = attribute_id;
}


static void readPatternNames(Controller *me, AttributeAction const *aa)
{
    uint8_t nr_of_patterns = Patterns_getCount();
    char const *pattern_names[nr_of_patterns];  // Reserve enough space.
    Patterns_getNames(pattern_names, nr_of_patterns);
    uint16_t nbtw = sizeof(PacketHeader) + sizeof(AttributeAction);
    uint16_t packet_size = nbtw + Matter_encodedStringArrayLength(pattern_names, nr_of_patterns);
    // TODO Ensure packet_size does not exceed max frame payload size.
    uint8_t packet[packet_size];
    initResponsePacket((PacketHeader *)packet);
    initAttributeAction((AttributeAction *)(packet + sizeof(PacketHeader)), aa->transaction_id, OC_REPORT_DATA, aa->attribute_id);
    nbtw += Matter_encodeStringArray(packet + nbtw, pattern_names, nr_of_patterns);
    DataLink_sendDatagram(me->datalink, packet, nbtw);
}


static void attributeChanged(Controller *me, AttributeId ai, uint16_t trans_id, ElementEncoding enc, uint8_t const *data, uint16_t data_size)
{
    // BSP_logf("Controller_%s(%hu) size=%hu\n", __func__, ai, data_size);
    uint16_t nbtw = sizeof(PacketHeader) + sizeof(AttributeAction);
    uint8_t packet[nbtw + Matter_encodedDataLength(enc, data_size)];
    initResponsePacket((PacketHeader *)packet);
    initAttributeAction((AttributeAction *)(packet + sizeof(PacketHeader)), trans_id, OC_REPORT_DATA, ai);
    nbtw += Matter_encode(packet + nbtw, enc, data, data_size);
    DataLink_sendDatagram(me->datalink, packet, nbtw);
}


static void sendStatusResponse(Controller *me, AttributeAction const *aa, StatusCode sc)
{
    if (sc != SC_SUCCESS) BSP_logf("%s %hu for attr id=%hu\n", __func__, sc, aa->attribute_id);
    uint16_t nbtw = sizeof(PacketHeader) + sizeof(AttributeAction);
    uint8_t packet[nbtw + Matter_encodedDataLength(EE_UNSIGNED_INT, 1)];
    initResponsePacket((PacketHeader *)packet);
    initAttributeAction((AttributeAction *)(packet + sizeof(PacketHeader)), aa->transaction_id, OC_STATUS_RESPONSE, aa->attribute_id);
    nbtw += Matter_encode(packet + nbtw, EE_UNSIGNED_INT, &sc, 1);
    DataLink_sendDatagram(me->datalink, packet, nbtw);
}


static void logTransaction(AttributeAction const *aa, char const *action_str)
{
    BSP_logf("Transaction %hu: %s attribute %hu\n", aa->transaction_id, action_str, aa->attribute_id);
}


static void handleReadRequest(Controller *me, AttributeAction const *aa)
{
    switch (aa->attribute_id)
    {
        case AI_FIRMWARE_VERSION: {
            char const *fw_version = BSP_firmwareVersion();
            attributeChanged(me, aa->attribute_id, aa->transaction_id, EE_UTF8_1LEN, (uint8_t const *)fw_version, strlen(fw_version));
            break;
        }
        case AI_VOLTAGES:
            Attribute_awaitRead(aa->attribute_id, aa->transaction_id, (AttrNotifier)&attributeChanged, me);
            BSP_triggerADC();
            break;
        case AI_CLOCK_MICROS: {
            uint64_t clock_micros = BSP_microsecondsSinceBoot();
            attributeChanged(me, aa->attribute_id, aa->transaction_id, EE_UNSIGNED_INT, (uint8_t const *)&clock_micros, sizeof clock_micros);
            break;
        }
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
        case AI_BOX_NAME:
            attributeChanged(me, aa->attribute_id, aa->transaction_id, EE_UTF8_1LEN, (uint8_t const *)me->box_name, strlen(me->box_name));
            break;
        case AI_PT_DESCRIPTOR_QUEUE:
            Sequencer_notifyPtQueue(me->sequencer, aa->transaction_id);
            break;
        default:
            BSP_logf("%s: unknown attribute id=%hu\n", __func__, aa->attribute_id);
            sendStatusResponse(me, aa, SC_UNSUPPORTED_ATTRIBUTE);
            return;
    }
}


static EventType eventTypeForCommand(uint8_t const *cs, uint16_t len)
{
    if (len == 4 && memcmp(cs, "stop",  len) == 0) return ET_STOP;
    if (len == 4 && memcmp(cs, "play",  len) == 0) return ET_PLAY;
    if (len == 5 && memcmp(cs, "pause", len) == 0) return ET_PAUSE;
    return ET_UNKNOWN_COMMAND;
}


static void updateBoxName(Controller *me, uint8_t const *name, uint16_t len)
{
    size_t nb = (len < sizeof me->box_name ? len : sizeof me->box_name - 1);
    memcpy(me->box_name, name, nb);
    me->box_name[nb] = '\0';
    // TODO Store the name in nonvolatile memory rather than in RAM.
    BSP_logf("Box name set to '%s'\n", me->box_name);
}


static void handleWriteRequest(Controller *me, AttributeAction const *aa)
{
    switch (aa->attribute_id)
    {
        case AI_CURRENT_PATTERN_NAME:
            if (aa->data[0] == EE_UTF8_1LEN) {
                EventQueue_postEvent((EventQueue *)me->sequencer, ET_SELECT_PATTERN_BY_NAME, aa->data + 2, aa->data[1]);
            }
            break;
        case AI_INTENSITY_PERCENT:
            if (aa->data[0] == EE_UNSIGNED_INT_1) {
                EventQueue_postEvent((EventQueue *)me->sequencer, ET_SET_INTENSITY, &aa->data[1], sizeof aa->data[1]);
            }
            break;
        case AI_PLAY_PAUSE_STOP:
            if (aa->data[0] == EE_UTF8_1LEN) {
                EventQueue_postEvent((EventQueue *)me->sequencer, eventTypeForCommand(aa->data + 2, aa->data[1]), NULL, 0);
            }
            break;
        case AI_BOX_NAME:
            if (aa->data[0] == EE_UTF8_1LEN) {
                updateBoxName(me, aa->data + 2, aa->data[1]);
            }
            break;
        case AI_PT_DESCRIPTOR_QUEUE:
            if (aa->data[0] == EE_BYTES_1LEN) {
                EventQueue_postEvent((EventQueue *)me->sequencer, ET_QUEUE_PULSE_TRAIN, aa->data + 2, aa->data[1]);
            }
            break;
        case AI_HEARTBEAT_INTERVAL_SECS:
            if (aa->data[0] == EE_UNSIGNED_INT_2) {
                uint16_t interval_secs = aa->data[1] | (aa->data[2] << 8);
                if (interval_secs > 3600) interval_secs = 3600;
                BSP_logf("Setting heartbeat interval to %hu seconds\n", interval_secs);
                me->heartbeat_interval_µs = interval_secs * 1000000UL;
            }
            break;
        default:
            BSP_logf("%s: unknown attribute id=%hu\n", __func__, aa->attribute_id);
            sendStatusResponse(me, aa, SC_UNSUPPORTED_ATTRIBUTE);
            return;
    }
    sendStatusResponse(me, aa, SC_SUCCESS);
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
        case AI_PT_DESCRIPTOR_QUEUE:
            if (aa->data[0] == EE_BOOLEAN_TRUE) {
                EventQueue_postEvent((EventQueue *)me->sequencer, ET_START_STREAM, NULL, 0);
            } else if (aa->data[0] == EE_BOOLEAN_FALSE) {
                EventQueue_postEvent((EventQueue *)me->sequencer, ET_STOP_STREAM, NULL, 0);
            }
            break;
        default:
            BSP_logf("%s: unknown attribute id=%hu\n", __func__, aa->attribute_id);
            sendStatusResponse(me, aa, SC_UNSUPPORTED_ATTRIBUTE);
            return;
    }
    sendStatusResponse(me, aa, SC_SUCCESS);
}


static void handleRequest(Controller *me, AttributeAction const *aa)
{
    switch (aa->opcode)
    {
        case OC_READ_REQUEST:
            logTransaction(aa, "read");
            handleReadRequest(me, aa);
            break;
        case OC_WRITE_REQUEST:
            // logTransaction(aa, "write");
            handleWriteRequest(me, aa);
            break;
        case OC_SUBSCRIBE_REQUEST:
            logTransaction(aa, "subscribe to");
            Attribute_subscribe(aa->attribute_id, aa->transaction_id, (AttrNotifier)&attributeChanged, me);
            handleReadRequest(me, aa);
            break;
        case OC_INVOKE_REQUEST:
            logTransaction(aa, "invoke");
            handleInvokeRequest(me, aa);
            break;
        default:
            BSP_logf("%s, unknown opcode 0x%02hhx\n", __func__, aa->opcode);
            sendStatusResponse(me, aa, SC_UNSUPPORTED_COMMAND);
    }
}


static void *stateNop(Controller *me, AOEvent const *evt)
{
    BSP_logf("Controller_%s unexpected event: %u\n", __func__, AOEvent_type(evt));
    return NULL;
}


static void *stateReady(Controller *me, AOEvent const *evt)
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
    strncpy(me->box_name, "Neostim Mean Machine", sizeof me->box_name - 1);
    me->sequencer = sequencer;
    me->datalink  = datalink;
    me->heartbeat_interval_µs = 15000000;
}


void Controller_start(Controller *me)
{
    me->state = &stateReady;
    me->state(me, AOEvent_newEntryEvent());
    DataLink_open(me->datalink, &me->event_queue);
    DataLink_awaitSync(me->datalink);
    BSP_logf("Welcome to Neostim!\n%s", welcome_msg);
}


bool Controller_handleEvent(Controller *me)
{
    return EventQueue_handleNextEvent(&me->event_queue, (EvtFunc)&dispatchEvent, me);
}


bool Controller_heartbeatElapsed(Controller const *me, uint32_t delta_µs)
{
    return me->heartbeat_interval_µs != 0 && delta_µs >= me->heartbeat_interval_µs;
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
