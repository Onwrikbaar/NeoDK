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

// This module implements:
#include "comms.h"


struct _Comms {
    uint8_t tx_buf_store[200];
    // uint8_t rx_buf_store[200];
    CircBuffer output_buffer;
    int channel_fd;
};


static void rxCallback(Comms *me, uint32_t ch)
{
    if (ch == '\n') {
        static uint8_t msg[] = "Hello!\n";
        Comms_write(me, msg, sizeof(msg) - 1);
    } else {
        BSP_logf("%s(0x%02x)\n", __func__, ch);
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
    // Once the buffer is empty, disable this callback.
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
    BSP_initComms();                            // Initialise the communication peripheral.
    if (me->channel_fd >= 0 || (me->channel_fd = BSP_openSerialPort("serial_1")) < 0) return false;

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
    me->channel_fd = -1;
}


void Comms_delete(Comms *me)
{
    free(me);
}
