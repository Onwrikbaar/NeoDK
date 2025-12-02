/*
 * bsp_comms.h
 *
 *  NOTICE (do not remove):
 *      This file is part of project NeoDK (https://github.com/Onwrikbaar/NeoDK).
 *      See https://github.com/Onwrikbaar/NeoDK/blob/main/LICENSE.txt for full license details.
 *
 *  Created on: 5 Apr 2025
 *      Author: mark
 *   Copyright  2025 Neostim™
 */

#ifndef INC_BSP_COMMS_H_
#define INC_BSP_COMMS_H_

#include "convenience.h"

void BSP_enableUartInterrupt(int intr);
void BSP_enableUsbInterrupt(void);
void BSP_initComms(void);
DeviceId BSP_openSerialPort(char const *name);
int BSP_closeSerialPort(int fd);

#endif
