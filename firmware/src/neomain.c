/*
 * neomain.c -- Firmware plumbing for the Neostim Development Kit.
 *
 *  Created on: Oct 20, 2023
 *      Author: mark
 *   Copyright  2023 Neostim
 */

#include <stdlib.h>

#include "bsp_dbg.h"
#include "bsp_mao.h"
#include "bsp_app.h"

#ifndef MICROSECONDS_PER_APP_TIMER_TICK
#define MICROSECONDS_PER_APP_TIMER_TICK 1000U   // For 1 kHz tick.
#endif


static void handleAppTimerTick(uint64_t app_timer_micros)
{
    static uint32_t tick_count = 0;

    if (++tick_count == 500) {
        BSP_toggleLED();
        tick_count = 0;
    }
}


static void onButtonToggle(void *me, uint32_t pushed)
{
    BSP_logf("Button %s\n", pushed ? "pushed" : "released");
}


static bool maySleep()
{
    return true;
}


static void setupAndRunApplication(char const *app_name)
{
    BSP_registerAppTimerHandler(&handleAppTimerTick, MICROSECONDS_PER_APP_TIMER_TICK);
    Selector button_selector;
    Selector_init(&button_selector, (Action)&onButtonToggle, NULL);
    BSP_registerButtonHandler(&button_selector);

    BSP_logf("Starting %s on NeoDK!\n", app_name);
    while (true) {
        BSP_idle(&maySleep);
    }
    BSP_logf("End of session\n");
}


int main(int argc, char const *argv[])
{
    BSP_initDebug();
    BSP_logf("Initialising...\n");
    BSP_init();                                 // Get the hardware ready.

    setupAndRunApplication("Demo 1");

    BSP_close();
    BSP_logf("Bye!\n");
    BSP_closeDebug();
    BSP_shutDown();
    return 0;
}
