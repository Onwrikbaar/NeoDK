/*
 * neodk_main.c -- Main program of the Neostim Development Kit basic firmware.
 *
 *  Created on: Oct 20, 2023
 *      Author: mark
 *   Copyright  2023 Neostim
 */

#include <stdlib.h>

#include "bsp_dbg.h"
#include "bsp_mao.h"
#include "bsp_app.h"
#include "elec_conf.h"


#ifndef MICROSECONDS_PER_APP_TIMER_TICK
#define MICROSECONDS_PER_APP_TIMER_TICK 1000    // For 1 kHz tick.
#endif
#define MAX_PULSE_WIDTH_MICROS           200


typedef struct {
    uint8_t outbuf_storage[200];
    int serial_fd;
} Comms;

typedef struct {
    uint8_t pulse_width_micros;
    uint8_t pace_millis;
    uint8_t nr_of_pulses;
} PulseTrain;


static void handleAppTimerTick(uint64_t app_timer_micros)
{
    static uint16_t millis = 0;

    if (++millis == 1000) {
        // TODO Once per second.
        millis = 0;
    }
}


static void rxCallback(Comms *me, uint32_t ch)
{
    if (ch == '\n') {
        BSP_doChannelAction(me->serial_fd, CA_TX_CB_ENABLE);
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
    static char const hello[] = "Hello!";
    static char const *cp = hello;

    if (*cp != '\0') {
        *dst = *cp++;
    } else {
        BSP_doChannelAction(me->serial_fd, CA_TX_CB_DISABLE);
        *dst = '\n';
        cp = hello;
    }
}


static void txErrorCallback(Comms *me, uint32_t tx_error)
{
    BSP_logf("%s(%u)\n", __func__, tx_error);
}


static bool setupComms(Comms *me)
{
    BSP_initComms();
    if (me->serial_fd >= 0 || (me->serial_fd = BSP_openSerialPort("serial_1")) < 0) return false;

    BSP_logf("Serial device id=%d\n", me->serial_fd);
    Selector rx_sel, rx_err_sel, tx_err_sel;
    Selector_init(&rx_sel, (Action)&rxCallback, me);
    Selector_init(&rx_err_sel, (Action)&rxErrorCallback, me);
    Selector_init(&tx_err_sel, (Action)&txErrorCallback, me);
    BSP_registerRxCallback(me->serial_fd, &rx_sel, &rx_err_sel);
    BSP_registerTxCallback(me->serial_fd, (void (*)(void *, uint8_t *))&txCallback, me, &tx_err_sel);
    BSP_doChannelAction(me->serial_fd, CA_OVERRUN_CB_ENABLE);
    BSP_doChannelAction(me->serial_fd, CA_FRAMING_CB_ENABLE);
    BSP_doChannelAction(me->serial_fd, CA_RX_CB_ENABLE);
    BSP_doChannelAction(me->serial_fd, CA_TX_CB_ENABLE);
    return true;
}


static void onButtonToggle(PulseTrain *pt, uint32_t pushed)
{
    BSP_selectElectrodeConfiguration(EC_AC_BD); // Turn all four electrodes on, for now.
    BSP_logf("Pulse width is %hhu µs\n", pt->pulse_width_micros);
    BSP_startPulseTrain(!pushed, pt->pace_millis, pt->pulse_width_micros, pt->nr_of_pulses);
    if (!pushed && pt->pulse_width_micros < MAX_PULSE_WIDTH_MICROS) {
        pt->pulse_width_micros += 4;
    }
}


static void setupAndRunApplication(char const *app_name)
{
    Selector button_selector;
    PulseTrain pulse_train = {
        .pulse_width_micros = 50,
        .pace_millis = 40,
        .nr_of_pulses = 5,
    };
    BSP_registerButtonHandler(Selector_init(&button_selector, (Action)&onButtonToggle, &pulse_train));
    BSP_registerAppTimerHandler(&handleAppTimerTick, MICROSECONDS_PER_APP_TIMER_TICK);
    BSP_registerPulseHandler(NULL);

    Comms comms = { .serial_fd = -1 };
    setupComms(&comms);

    BSP_logf("Starting %s on NeoDK!\n", app_name);
    BSP_logf("Push the button! :-)\n");
    while (true) {
        BSP_idle(NULL);
    }
    BSP_logf("End of session\n");
}


int main()
{
    BSP_initDebug();
    BSP_logf("Initialising...\n");
    BSP_init();                                 // Get the hardware ready.

    setupAndRunApplication("Button Demo");

    BSP_close();
    BSP_logf("Bye!\n");
    BSP_closeDebug();
    BSP_shutDown();
    return 0;
}
