/*
 * comms.c
 *
 *  Created on: 24 Feb 2024
 *      Author: mark
 *   Copyright  2024 Neostim
 */

#include <stdlib.h>

#include "bsp_dbg.h"
#include "bsp_mao.h"
#include "bsp_app.h"
#include "circbuffer.h"
#include "net_frame.h"

// This module implements:
#include "comms.h"

#define MAX_PAYLOAD_SIZE     1024

struct _Comms {
    uint8_t tx_buf_store[200];
    // uint8_t rx_buf_store[200];
    CircBuffer output_buffer;
    uint8_t *rx_frame_buffer;
    uint8_t rx_nb;
    DeviceId channel_fd;
    uint16_t header_size;
    uint16_t rx_payload_size;
    uint8_t synced;
    uint8_t seq_nr;
};


/*static*/ void dumpBuffer(const char *prefix, const uint8_t *bbuf, uint8_t nb)
{
    if (prefix != NULL) BSP_logf("%s", prefix);
    for (uint8_t i = 0; i < nb; i++) {
        BSP_logf(" 0x%02x", bbuf[i]);
    }
}


static void makeRoomForNextByte(Comms *me)
{
    me->rx_nb -= 1;
    for (uint8_t i = 0; i < me->rx_nb; i++) {
        me->rx_frame_buffer[i] = me->rx_frame_buffer[i + 1];
    }
}


static FrameType assembleIncomingFrame(Comms *me, uint8_t ch)
{
    me->rx_frame_buffer[me->rx_nb++] = ch;
    // Just collect bytes until we have a full header.
    if (me->rx_nb < me->header_size) return FT_NONE;

    PhysFrame const *frame = (PhysFrame const *)me->rx_frame_buffer;
    // Shift/append the input buffer until it contains a valid header.
    if (me->rx_nb == me->header_size) {
        if (! PhysFrame_hasValidHeader(frame)) {
            makeRoomForNextByte(me);
            return FT_NONE;
        }
        // Valid header received, start collecting the payload (if any).
        me->rx_payload_size = PhysFrame_payloadSize(frame);
    }
    if (me->rx_nb == me->header_size + me->rx_payload_size) {
        me->rx_nb = 0;                          // Prepare for the next frame.
        return PhysFrame_type(frame);
    }
    return FT_NONE;
}


static void respondWithAckFrame(Comms *me, uint8_t ack_nr)
{
    uint8_t ack_frame[me->header_size];
    PhysFrame_initHeaderWithAck((PhysFrame *)ack_frame, FT_ACK, me->seq_nr, ack_nr);
    // BSP_logf("%s(%hhu) to controller\n", __func__, ack_nr);
    Comms_write(me, ack_frame, sizeof ack_frame);
}


static void handleIncomingFrame(Comms *me, PhysFrame const *frame)
{
    uint8_t rx_seq_nr = PhysFrame_seqNr(frame);
    uint16_t payload_size = PhysFrame_payloadSize(frame);
    BSP_logf("Got %s frame, seq_nr=%2hhu, payload_size=%hu\n",
                PhysFrame_typeName(PhysFrame_type(frame)), rx_seq_nr, payload_size);
    respondWithAckFrame(me, rx_seq_nr);
}


static void rxCallback(Comms *me, uint32_t ch)
{
    if (! me->synced) {
        if (ch == '\n') {                       // Dweeb's poll character.
            respondWithAckFrame(me, ch);
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


static void rxErrorCallback(Comms *me, uint32_t rx_error)
{
    BSP_logf("%s(%u)\n", __func__, rx_error);
}


static void txCallback(Comms *me, uint8_t *dst)
{
    uint32_t nbr = CircBuffer_read(&me->output_buffer, dst, 1);
    M_ASSERT(nbr == 1);
    // Once the buffer is empty, disable this callback (atomic).
    BSP_criticalSectionEnter();
    if (CircBuffer_availableData(&me->output_buffer) == 0) {
        BSP_doChannelAction(me->channel_fd, CA_TX_CB_DISABLE);
    }
    BSP_criticalSectionExit();
}


static void txErrorCallback(Comms *me, uint32_t tx_error)
{
    BSP_logf("%s(%u)\n", __func__, tx_error);
}

/*
 * Below are the functions implementing this module's interface.
 */

Comms *Comms_new()
{
    Comms *me = (Comms *)malloc(sizeof(Comms));
    CircBuffer_init(&me->output_buffer, me->tx_buf_store, sizeof me->tx_buf_store);
    me->channel_fd = -1;
    return me;
}


bool Comms_open(Comms *me)
{
    M_ASSERT(me->channel_fd < 0);
    me->header_size = PhysFrame_headerSize();
    me->rx_frame_buffer = malloc(me->header_size + MAX_PAYLOAD_SIZE);
    me->rx_nb = 0;
    me->synced = 0;
    me->seq_nr = 0;

    BSP_initComms();                            // Initialise the communication peripheral.
    if ((me->channel_fd = BSP_openSerialPort("serial_1")) < 0) return false;

    Selector rx_sel, rx_err_sel, tx_err_sel;
    Selector_init(&rx_sel, (Action)&rxCallback, me);
    Selector_init(&rx_err_sel, (Action)&rxErrorCallback, me);
    Selector_init(&tx_err_sel, (Action)&txErrorCallback, me);
    BSP_registerRxCallback(me->channel_fd, &rx_sel, &rx_err_sel);
    BSP_registerTxCallback(me->channel_fd, (void (*)(void *, uint8_t *))&txCallback, me, &tx_err_sel);
    BSP_doChannelAction(me->channel_fd, CA_OVERRUN_CB_ENABLE);
    BSP_doChannelAction(me->channel_fd, CA_FRAMING_CB_ENABLE);
    BSP_doChannelAction(me->channel_fd, CA_RX_CB_ENABLE);
    return true;
}


void Comms_waitForSync(Comms *me)
{
    BSP_logf("%s\n", __func__);
    me->synced = false;
}


uint32_t Comms_write(Comms *me, uint8_t const *data, size_t nb)
{
    BSP_criticalSectionEnter();
    uint32_t nbw = CircBuffer_write(&me->output_buffer, data, nb);
    if (nbw != 0) BSP_doChannelAction(me->channel_fd, CA_TX_CB_ENABLE);
    BSP_criticalSectionExit();
    return nbw;
}


void Comms_close(Comms *me)
{
    BSP_doChannelAction(me->channel_fd, CA_CLOSE);
    free(me->rx_frame_buffer);
    me->channel_fd = -1;
}


void Comms_delete(Comms *me)
{
    M_ASSERT(me->channel_fd < 0);
    free(me);
}
