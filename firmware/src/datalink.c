/*
 * datalink.c
 *
 *  NOTICE (do not remove):
 *      This file is part of project NeoDK (https://github.com/Onwrikbaar/NeoDK).
 *      See https://github.com/Onwrikbaar/NeoDK/blob/main/LICENSE.txt for full license details.
 *
 *  Created on: 24 Feb 2024
 *      Author: mark
 *   Copyright  2024, 2025 Neostim™
 */

#include <stdlib.h>

#include "bsp_dbg.h"
#include "bsp_mao.h"
#include "bsp_app.h"
#include "app_event.h"
#include "net_frame.h"
#include "debug_cli.h"                          // Temporary.

// This module implements:
#include "datalink.h"

#define MAX_PAYLOAD_SIZE    512

typedef struct {
    uint8_t  frame[FRAME_HEADER_SIZE + MAX_PAYLOAD_SIZE];
    uint32_t frame_timestamp_µs;
} TxFrameInfo;

struct _DataLink {
    EventQueue *delegate_queue;
    TxFrameInfo tx_frame_info[NR_OF_FRAME_SEQ_NRS];
    CircBuffer output_buffer;
    uint8_t tx_buf_store[600];
    uint8_t rx_frame_buffer[FRAME_HEADER_SIZE + MAX_PAYLOAD_SIZE];
    uint8_t rx_nb;
    DeviceId channel_fd;
    uint16_t rx_payload_size;
    uint8_t tx_seq_nr;
    uint8_t synced;
};


static void init(DataLink *me)
{
    me->rx_nb = 0;
    me->synced = false;
    me->tx_seq_nr = 0;
}


static void makeRoomForNextByte(DataLink *me)
{
    me->rx_nb -= 1;
    for (uint8_t i = 0; i < me->rx_nb; i++) {
        me->rx_frame_buffer[i] = me->rx_frame_buffer[i + 1];
    }
}


static FrameType assembleIncomingFrame(DataLink *me, uint8_t ch)
{
    me->rx_frame_buffer[me->rx_nb++] = ch;
    // Just collect bytes until we have a full header.
    if (me->rx_nb < FRAME_HEADER_SIZE) return FT_NONE;

    PhysFrame const *frame = (PhysFrame const *)me->rx_frame_buffer;
    // Shift/append the input buffer until it contains a valid header.
    if (me->rx_nb == FRAME_HEADER_SIZE) {
        if (! PhysFrame_hasValidHeader(frame)) {
            makeRoomForNextByte(me);
            return FT_NONE;
        }
        // Valid header received, start collecting the payload (if any).
        if ((me->rx_payload_size = PhysFrame_payloadSize(frame)) > MAX_PAYLOAD_SIZE) {
            // Report and skip this too large frame and search for a new valid header.
            BSP_logf("Frame payload too big: %hu bytes", me->rx_payload_size);
            me->rx_payload_size = 0;
            return FT_NONE;
        }
    }
    if (me->rx_nb == FRAME_HEADER_SIZE + me->rx_payload_size) {
        me->rx_nb = 0;                          // Prepare for the next frame.
        return PhysFrame_type(frame);
    }
    return FT_NONE;
}


static uint32_t writeFrame(DataLink *me, uint8_t const *frame, uint32_t nb)
{
    BSP_criticalSectionEnter();
    uint32_t nbw = CircBuffer_write(&me->output_buffer, frame, nb);
    if (nbw != 0) BSP_doChannelAction(me->channel_fd, CA_TX_CB_ENABLE);
    BSP_criticalSectionExit();
    return nbw;
}


static void respondWithAckFrame(DataLink *me, uint8_t ack_nr, NetworkServiceType nst)
{
    uint8_t ack_frame[FRAME_HEADER_SIZE];
    // TODO Check whether we have an outbound frame queued that
    // we can piggyback on instead of sending a separate ACK frame.
    PhysFrame_initHeaderWithAck((PhysFrame *)ack_frame, FT_ACK, 0, ack_nr, nst);
    // BSP_logf("%s(%hhu) to controller\n", __func__, ack_nr);
    writeFrame(me, ack_frame, sizeof ack_frame);
}


static void handleIncomingDataFrame(DataLink *me, PhysFrame const *frame)
{
    NetworkServiceType nst = PhysFrame_serviceType(frame);
    uint8_t const *payload = PhysFrame_payload(frame);
    uint16_t payload_size  = PhysFrame_payloadSize(frame);
    if (nst == NST_DEBUG) {
        // BSP_logf("Got debug frame, seq_nr=%hhu, command length=%hu\n", PhysFrame_seqNr(frame), payload_size);
        CLI_handleRemoteInput(payload, payload_size);
    } else {
        // BSP_logf("Got packet frame, seq_nr=%hhu, packet size=%hu\n", PhysFrame_seqNr(frame), payload_size);
        EventQueue_postEvent(me->delegate_queue, ET_INCOMING_PACKET, payload, payload_size);
    }
}


static void setFrameQueuedTimestamp(DataLink *me)
{
    me->tx_frame_info[me->tx_seq_nr].frame_timestamp_µs = BSP_microsecondsSinceBoot();
}


static uint32_t getFrameAckDelay(DataLink *me, uint8_t ack_nr)
{
    return BSP_microsecondsSinceBoot() - me->tx_frame_info[ack_nr].frame_timestamp_µs;
}


static void handleIncomingFrame(DataLink *me, PhysFrame const *frame)
{
    if (! PhysFrame_isIntact(frame)) {
        BSP_logf("%s: bad frame\n", __func__);
        // Do not ACK, wait for retransmission.
        // Can also NAK if the header is valid but payload corrupt.
        return;
    }

    FrameType frame_type = PhysFrame_type(frame);
    if (frame_type == FT_ACK) {
        uint8_t ack_nr = PhysFrame_ackNr(frame);
        __attribute__((unused))                 // Temporary.
        uint32_t ack_delay = getFrameAckDelay(me, ack_nr);
        // BSP_logf("Got ACK for frame %hhu after %u µs\n", ack_nr, ack_delay);
        // TODO Slide the window.
        return;
    }

    uint8_t rx_seq_nr = PhysFrame_seqNr(frame);
    NetworkServiceType nst = PhysFrame_serviceType(frame);
    if (frame_type == FT_SYNC) {
        BSP_logf("Got SYNC frame, seq_nr=%hhu\n", rx_seq_nr);
        respondWithAckFrame(me, rx_seq_nr, nst);
        EventQueue_postEvent(me->delegate_queue, nst == NST_DEBUG ? ET_DEBUG_SYNC : ET_DATAGRAM_SYNC, NULL, 0);
        return;
    }

    uint16_t payload_size = PhysFrame_payloadSize(frame);
    if (frame_type == FT_DATA) {
        handleIncomingDataFrame(me, frame);
        respondWithAckFrame(me, rx_seq_nr, nst);
    } else {
        BSP_logf("Got %s frame, seq_nr=%hhu, payload_size=%hu\n",
                    PhysFrame_frameTypeName(frame_type), rx_seq_nr, payload_size);
    }
}


static void rxCallback(DataLink *me, uint32_t ch)
{
    if (! me->synced) {
        if (ch == '\n') {                       // Dweeb's poll character.
            respondWithAckFrame(me, 0x7, NST_DATAGRAM);
            me->synced = true;
        } else if (assembleIncomingFrame(me, (uint8_t)ch) == FT_SYNC) {
            // BSP_logf("Byte %2hu is 0x%02x\n", me->rx_nb, ch);
            handleIncomingFrame(me, (PhysFrame const *)me->rx_frame_buffer);
            me->synced = true;
        }
    } else {
        if (assembleIncomingFrame(me, (uint8_t)ch) != FT_NONE) {
            handleIncomingFrame(me, (PhysFrame const *)me->rx_frame_buffer);
        }
    }
}


static void rxErrorCallback(DataLink *me, uint32_t rx_error)
{
    BSP_logf("%s(%u)\n", __func__, rx_error);
}


static void txCallback(DataLink *me, uint8_t *dst)
{
    BSP_criticalSectionEnter();
    uint32_t nbr = CircBuffer_read(&me->output_buffer, dst, 1);
    M_ASSERT(nbr == 1);
    // Once the buffer is empty, disable this callback (atomic).
    if (CircBuffer_availableData(&me->output_buffer) == 0) {
        BSP_doChannelAction(me->channel_fd, CA_TX_CB_DISABLE);
    }
    BSP_criticalSectionExit();
}


static void txErrorCallback(DataLink *me, uint32_t tx_error)
{
    BSP_logf("%s(%u)\n", __func__, tx_error);
}


static void setupChannel(DataLink *me)
{
    Selector rx_sel, rx_err_sel, tx_err_sel;
    Selector_init(&rx_sel, (Action)&rxCallback, me);
    Selector_init(&rx_err_sel, (Action)&rxErrorCallback, me);
    Selector_init(&tx_err_sel, (Action)&txErrorCallback, me);
    BSP_registerRxCallback(me->channel_fd, &rx_sel, &rx_err_sel);
    BSP_registerTxCallback(me->channel_fd, (void (*)(void *, uint8_t *))&txCallback, me, &tx_err_sel);
    BSP_doChannelAction(me->channel_fd, CA_OVERRUN_CB_ENABLE);
    BSP_doChannelAction(me->channel_fd, CA_FRAMING_CB_ENABLE);
    BSP_doChannelAction(me->channel_fd, CA_RX_CB_ENABLE);
}


static bool sendPacket(DataLink *me, NetworkServiceType nst, uint8_t const *packet, uint16_t nb)
{
    uint8_t *frame_store = me->tx_frame_info[me->tx_seq_nr].frame;
    PhysFrame_init((PhysFrame *)frame_store, FT_DATA, me->tx_seq_nr, nst, packet, nb);
    if (writeFrame(me, frame_store, FRAME_HEADER_SIZE + nb) == FRAME_HEADER_SIZE + nb) {
        setFrameQueuedTimestamp(me);
        me->tx_seq_nr = (me->tx_seq_nr + 1) & 0x7;
        return true;
    }
    // TODO Schedule for retry.
    return false;
}

/*
 * Below are the functions implementing this module's interface.
 */

DataLink *DataLink_new()
{
    DataLink *me = (DataLink *)malloc(sizeof(DataLink));
    CircBuffer_init(&me->output_buffer, me->tx_buf_store, sizeof me->tx_buf_store);
    me->channel_fd = -1;
    return me;
}


bool DataLink_open(DataLink *me, EventQueue *dq)
{
    M_ASSERT(me->channel_fd < 0);               // Make sure this link is inactive.
    me->delegate_queue = dq;
    init(me);

    BSP_initComms();                            // Initialise the communication peripheral.
    if ((me->channel_fd = BSP_openSerialPort("serial_1")) < 0) return false;

    setupChannel(me);
    return true;
}


void DataLink_awaitSync(DataLink *me)
{
    me->synced = false;
}


bool DataLink_sendDebugPacket(DataLink *me, uint8_t const *packet, uint16_t nb)
{
    return sendPacket(me, NST_DEBUG, packet, nb);
}


bool DataLink_sendDatagram(DataLink *me, uint8_t const *packet, uint16_t nb)
{
    return sendPacket(me, NST_DATAGRAM, packet, nb);
}


void DataLink_close(DataLink *me)
{
    BSP_doChannelAction(me->channel_fd, CA_CLOSE);
    me->channel_fd = -1;
}


void DataLink_delete(DataLink *me)
{
    M_ASSERT(me->channel_fd < 0);
    free(me);
}
