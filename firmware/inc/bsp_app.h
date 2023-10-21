/*
 * bsp_app.h -- The application's Board Support Package interface.
 *
 *  Created on: 6 Oct 2020
 *      Author: mark
 *   Copyright  2020..2023 Neostim
 */

#ifndef INC_BSP_APP_H_
#define INC_BSP_APP_H_

#include "convenience.h"


void BSP_init();                                // Get the hardware ready for action.
uint32_t BSP_millisecondsToTicks(uint16_t ms);
uint64_t BSP_ticksToMicroseconds(uint64_t ticks);
uint64_t BSP_microsecondsSinceBoot();

// To install some handlers.
void BSP_registerAppTimerHandler(void (*)(uint64_t), uint32_t microseconds_per_app_timer_tick);
void BSP_registerIdleHandler(Selector *);
void BSP_registerButtonHandler(Selector *);

// Miscellaneous.
void BSP_toggleLED();

// Serial comms related functions.
void BSP_initComms();
DeviceId BSP_openSerialPort(char const *name);
int BSP_closeSerialPort(int fd);

// Pulse generation related functions.
void BSP_setSwitches(uint16_t pattern);
void BSP_startPulseTrain(uint8_t phase, uint8_t pace_ms, uint8_t pulse_width_micros, uint16_t nr_of_pulses);

// Firmware update.
void BSP_gotoDfuMode();

// Close any communication ports and release resources, if applicable.
void BSP_close();
void BSP_shutDown();

#endif
