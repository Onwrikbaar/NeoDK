/*
 * bsp_app.h -- The application's Board Support Package interface.
 *
 *  NOTICE (do not remove):
 *      This file is part of project NeoDK (https://github.com/Onwrikbaar/NeoDK).
 *      See https://github.com/Onwrikbaar/NeoDK/blob/main/LICENSE.txt for full license details.
 *
 *  Created on: 6 Oct 2020
 *      Author: mark
 *   Copyright  2020..2024 Neostim™
 */

#ifndef INC_BSP_APP_H_
#define INC_BSP_APP_H_

#include "convenience.h"
#include "eventqueue.h"

typedef struct {
    uint8_t elcon[2];
    uint8_t pulse_width_micros;
    uint8_t pace_ms;
    uint8_t nr_of_pulses;
} PulseTrain;


void BSP_init(void);                            // Get the hardware ready for action.
void BSP_registerPulseDelegate(EventQueue *);
void BSP_toggleTheLED(void);
uint32_t BSP_millisecondsToTicks(uint16_t ms);
uint64_t BSP_ticksToMicroseconds(uint64_t ticks);
uint64_t BSP_microsecondsSinceBoot(void);

// To install some handlers.
void BSP_registerIdleHandler(Selector *);
void BSP_registerButtonHandler(Selector *);

// Serial communications related functions.
void BSP_initComms(void);
DeviceId BSP_openSerialPort(char const *name);
int BSP_closeSerialPort(int fd);

// Pulse generation related functions.
uint16_t BSP_setPrimaryVoltage_mV(uint16_t V_prim_mV);
void BSP_primaryVoltageEnable(bool must_be_on);
bool BSP_startPulseTrain(PulseTrain const *);

// Debugging stuff.
void BSP_triggerADC(void);

// Firmware update.
void BSP_gotoDfuMode(void);

// Close any communication ports and release resources, if applicable.
void BSP_close(void);
void BSP_sleepMCU(void);
void BSP_shutDown(void);

#endif
