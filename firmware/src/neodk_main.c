/*
 * neodk_main.c -- Main program of the Neostim Development Kit demo firmware.
 *
 *  Created on: Oct 20, 2023
 *      Author: mark
 *   Copyright  2023, 2024 Neostim
 */

#include <stdlib.h>

#include "bsp_dbg.h"
#include "bsp_mao.h"
#include "bsp_app.h"
#include "comms.h"
#include "sequencer.h"


#ifndef MICROSECONDS_PER_APP_TIMER_TICK
#define MICROSECONDS_PER_APP_TIMER_TICK 1000    // For 1 kHz tick.
#endif


static void handleAppTimerTick(uint64_t app_timer_micros)
{
    static uint16_t millis = 0;

    if (++millis == 5000) {
        BSP_logf("%s\n", __func__);
        millis = 0;
    }
}


static void onButtonToggle(Sequencer *sequencer, bool pushed)
{
    Sequencer_startPulseTrain(sequencer, !pushed);
}


static void setupAndRunApplication(char const *app_name)
{
    BSP_registerAppTimerHandler(&handleAppTimerTick, MICROSECONDS_PER_APP_TIMER_TICK);

    Comms *comms = Comms_new();
    Comms_open(comms);

    Sequencer *sequencer = Sequencer_new();
    Selector button_selector;
    BSP_registerButtonHandler(Selector_init(&button_selector, (Action)&onButtonToggle, sequencer));

    BSP_logf("Starting %s on NeoDK!\n", app_name);
    BSP_logf("Push the button! :-)\n");
    BSP_setPrimaryVoltage_mV(2000);
    while (true) {
        BSP_idle(NULL);
    }
    BSP_logf("End of session\n");

    Sequencer_delete(sequencer);
    Comms_close(comms);
    Comms_delete(comms);
}


int main()
{
    BSP_initDebug();
    BSP_logf("Initialising...\n");
    BSP_init();                                 // Get the hardware ready.

    setupAndRunApplication("Button Shock Demo");

    BSP_close();
    BSP_logf("Bye!\n");
    BSP_closeDebug();
    BSP_shutDown();
    return 0;
}
