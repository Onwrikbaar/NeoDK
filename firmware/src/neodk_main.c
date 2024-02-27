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
#include "app_event.h"
#include "comms.h"


#ifndef MICROSECONDS_PER_APP_TIMER_TICK
#define MICROSECONDS_PER_APP_TIMER_TICK 1000    // For 1 kHz tick.
#endif
#define MAX_PULSE_WIDTH_MICROS           200


typedef struct {
    uint8_t pulse_width_micros;
    uint8_t pace_millis;
    uint8_t nr_of_pulses;
} PulseTrain;

typedef struct {
    volatile bool busy;
} Sequencer;


static void handleAppTimerTick(uint64_t app_timer_micros)
{
    static uint16_t millis = 0;

    if (++millis == 1000) {
        // TODO Once per second.
        millis = 0;
    }
}


static void onButtonToggle(PulseTrain *pt, uint32_t pushed)
{
    BSP_selectElectrodeConfiguration(0x5, 0xa); // Turn all four electrodes on, for now.
    BSP_logf("\nPulse width is %hhu µs\n", pt->pulse_width_micros);
    BSP_startPulseTrain(!pushed, pt->pace_millis, pt->pulse_width_micros, pt->nr_of_pulses);
    if (!pushed && pt->pulse_width_micros < MAX_PULSE_WIDTH_MICROS) {
        pt->pulse_width_micros += 4;
    }
}


static void pulseTrainFinished(Sequencer *me, uint32_t evt)
{
    BSP_logf("%s\n", __func__);
    me->busy = false;
}


static void setupAndRunApplication(char const *app_name)
{
    static PulseTrain pulse_train = {
        .pulse_width_micros = 50,
        .pace_millis = 40,
        .nr_of_pulses = 5,
    };

    Selector button_selector;
    BSP_registerButtonHandler(Selector_init(&button_selector, (Action)&onButtonToggle, &pulse_train));
    BSP_registerAppTimerHandler(&handleAppTimerTick, MICROSECONDS_PER_APP_TIMER_TICK);

    Selector sequencer_selector;
    Sequencer sequencer = { .busy = false };
    BSP_registerPulseHandler(Selector_init(&sequencer_selector, (Action)&pulseTrainFinished, &sequencer));

    Comms *comms = Comms_new();
    Comms_open(comms);

    BSP_logf("Starting %s on NeoDK!\n", app_name);
    BSP_logf("Push the button! :-)\n");
    BSP_setPrimaryVoltage_mV(2000);
    while (true) {
        BSP_idle(NULL);
    }
    BSP_logf("End of session\n");

    Comms_close(comms);
    Comms_delete(comms);
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
