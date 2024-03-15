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


static void onAppTimerTick(uint64_t app_timer_micros)
{
    static uint64_t prev_micros = 0;

    if (app_timer_micros - prev_micros >= 15000000) {
        uint32_t seconds_since_boot = (uint32_t)(app_timer_micros / 1000000UL);
        BSP_logf("Time %02u:%02u:%02u\n", seconds_since_boot / 3600, (seconds_since_boot / 60) % 60, seconds_since_boot % 60);
        prev_micros = app_timer_micros;
    }
}


static void setupAndRunApplication(char const *app_name)
{
    Comms *comms = Comms_new();
    Comms_open(comms);

    Sequencer *sequencer = Sequencer_new();
    Sequencer_start(sequencer);
    Selector button_selector;                   // Pass button events to the sequencer.
    BSP_registerButtonHandler(Selector_init(&button_selector, (Action)&Sequencer_togglePlayPause, sequencer));

    BSP_logf("Starting %s on NeoDK!\n", app_name);
    BSP_logf("Push the button to play or pause! :-)\n");
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

    BSP_registerAppTimerHandler(&onAppTimerTick, MICROSECONDS_PER_APP_TIMER_TICK);
    setupAndRunApplication("Moving Pattern Demo");

    BSP_close();
    BSP_logf("Bye!\n");
    BSP_closeDebug();
    BSP_shutDown();
    return 0;
}
