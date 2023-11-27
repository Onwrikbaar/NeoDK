/*
 * neomain.c -- Main program of the Neostim Development Kit basic firmware.
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
#define MICROSECONDS_PER_APP_TIMER_TICK 1000    // For 1 kHz tick.
#endif
#define MAX_PULSE_WIDTH_MICROS           200


typedef struct {
    uint8_t pulse_width_micros;
    uint8_t pace_millis;
    uint8_t nr_of_pulses;
} PulseTrain;


static void handleAppTimerTick(uint64_t app_timer_micros)
{
    static uint32_t tick_count = 0;

    if (++tick_count == 500) {                  // Every half second.
        BSP_toggleLED();
        tick_count = 0;
    }
}


static void onButtonToggle(PulseTrain *pt, uint32_t pushed)
{
    BSP_startPulseTrain(pushed, pt->pace_millis, pt->pulse_width_micros, pt->nr_of_pulses);
    if (!pushed && pt->pulse_width_micros < MAX_PULSE_WIDTH_MICROS) {
        pt->pulse_width_micros += 1;
    }
}


static void setupAndRunApplication(char const *app_name)
{
    BSP_registerAppTimerHandler(&handleAppTimerTick, MICROSECONDS_PER_APP_TIMER_TICK);

    Selector button_selector;
    PulseTrain pulse_train = {
        .pulse_width_micros = 50,
        .pace_millis = 10,
        .nr_of_pulses = 5,
    };
    BSP_registerButtonHandler(Selector_init(&button_selector, (Action)&onButtonToggle, &pulse_train));
    BSP_initComms();

    BSP_logf("Starting %s on NeoDK!\n", app_name);
    BSP_logf("Push the button! :-)\n");
    while (true) {
        BSP_idle(NULL);
    }
    BSP_logf("End of session\n");
}


int main(int argc, char const *argv[])
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
