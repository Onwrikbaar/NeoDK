/*
 * sequencer.c -- the pulse train executor
 *
 *  Created on: 27 Feb 2024
 *      Author: mark
 *   Copyright  2024 Neostim
 */

#include <stdlib.h>

#include "bsp_dbg.h"
#include "bsp_app.h"
#include "app_event.h"
#include "convenience.h"

// This module implements:
#include "sequencer.h"

#define MAX_PULSE_WIDTH_MICROS           200


typedef struct {
    uint8_t pulse_width_micros;
    uint8_t pace_millis;
    uint8_t nr_of_pulses;
} PulseTrain;

struct _Sequencer {
    uint8_t state;
    volatile uint8_t busy;
};


static void handleEvent(Sequencer *me, EventType evt)
{
    switch (evt)
    {
        case 0:
            break;
        case 1:
            BSP_logf("Pulse train finished\n");
            me->busy = false;
            break;
        default:
            BSP_logf("Sequencer_%s unknown event: %u\n", __func__, evt);
    }
}

/*
 * Below are the functions implementing this module's interface.
 */

Sequencer *Sequencer_new()
{
    Sequencer *me = (Sequencer *)malloc(sizeof(Sequencer));
    me->state = 0;
    me->busy = false;

    Selector sequencer_selector;
    BSP_registerPulseHandler(Selector_init(&sequencer_selector, (Action)&handleEvent, me));
    return me;
}


void Sequencer_startPulseTrain(Sequencer *me, uint8_t phase)
{
    static PulseTrain pt = {
        .pulse_width_micros = 50,
        .pace_millis = 40,
        .nr_of_pulses = 5,
    };

    BSP_selectElectrodeConfiguration(0x5, 0xa); // Turn all four electrodes on, for now.
    BSP_logf("\nPulse width is %hhu µs\n", pt.pulse_width_micros);
    me->busy = true;
    BSP_startPulseTrain(phase, pt.pace_millis, pt.pulse_width_micros, pt.nr_of_pulses);
    if (phase == 1 && pt.pulse_width_micros < MAX_PULSE_WIDTH_MICROS) {
        pt.pulse_width_micros += 4;
    }
}


void Sequencer_delete(Sequencer *me)
{
    // TODO Disable the pulse electronics.
    free(me);
}
