/*
 * debug_cli.c
 *
 *  NOTICE (do not remove):
 *      This file is part of project NeoDK (https://github.com/Onwrikbaar/NeoDK).
 *      See https://github.com/Onwrikbaar/NeoDK/blob/main/LICENSE.txt for full license details.
 *
 *  Created on: 26 Mar 2024
 *      Author: mark
 *   Copyright  2024 Neostim™
 */

#include <stdio.h>

#include "bsp_dbg.h"
#include "bsp_mao.h"
#include "bsp_app.h"
#include "app_event.h"

// This module implements:
#include "debug_cli.h"


typedef struct {
    uint8_t buf[20];
    EventQueue *delegate_queue;
    DataLink *datalink;
    uint16_t V_prim_mV;
    uint8_t state;
    uint8_t len;
} CmndInterp;

static CmndInterp my = {0};


static void gatherInputCharacters(CmndInterp *me, char ch)
{
    me->buf[me->len] = '\0';
    if (ch == '\n' || (me->buf[me->len++] = ch, me->len == sizeof me->buf)) {
        if (ch != '\n') BSP_logf("\n");         // Buffer is full, no newline seen.
        BSP_logf("Console: '%s'\n", me->buf);
        me->len = 0;
    }
}


static void setPrimaryVoltage_mV(CmndInterp *me, uint16_t mV)
{
    me->V_prim_mV = BSP_setPrimaryVoltage_mV(mV);
    uint8_t pulse_width = 40 + me->V_prim_mV / 100;
    EventQueue_postEvent(me->delegate_queue, ET_SET_PULSE_WIDTH, &pulse_width, sizeof pulse_width);
}


static void changePrimaryVoltage_mV(CmndInterp *me, int16_t delta_mV)
{
    int32_t soll_mV = (int32_t)me->V_prim_mV + delta_mV;
    if (soll_mV < 1000) soll_mV = 1000;
    else if (soll_mV > 10200) soll_mV = 10200;
    setPrimaryVoltage_mV(me, soll_mV);
}

/**
 * @brief   Console commands for interactive testing.
 */
static void interpretCommand(CmndInterp *me, char ch)
{
    switch (ch)
    {
        case '?':
            CLI_logf("Commands: /? /a /b /d /l /n /q /u /v /w /0 /1../9\n");
            break;
        case '0':
            BSP_primaryVoltageEnable(false);
            setPrimaryVoltage_mV(me, 0);
            break;
        case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
            setPrimaryVoltage_mV(me, (ch - '0') * 1000U);
            BSP_primaryVoltageEnable(true);     // Turn on the buck converter.
            break;
        case 'a':
            BSP_triggerADC();
            break;
        case 'b':                               // Simulate a button press.
            EventQueue_postEvent(me->delegate_queue, ET_BUTTON_PUSHED, NULL, 0);
            EventQueue_postEvent(me->delegate_queue, ET_BUTTON_RELEASED, NULL, 0);
            break;
        case 'd':                               // Intensity down.
            changePrimaryVoltage_mV(me, -100);
            break;
        case 'l':
            BSP_toggleTheLED();
            break;
        case 'n':
            EventQueue_postEvent(me->delegate_queue, ET_SELECT_NEXT_PATTERN, NULL, 0);
            break;
        case 'q': {                             // Quit.
            int sig = 2;                        // Simulate Ctrl-C.
            EventQueue_postEvent(me->delegate_queue, ET_POSIX_SIGNAL, (uint8_t const *)&sig, sizeof sig);
            break;
        }
        case 'u':                               // Intensity up.
            changePrimaryVoltage_mV(me, +100);
            break;
        case 'v':
            CLI_logf("Firmware v0.32-beta\n");
            break;
        case 'w':                               // Allow rediscovery by Dweeb.
            DataLink_waitForSync(me->datalink);
            break;
        default:
            CLI_logf("Unknown command '/%c'\n", ch);
    }
}


static void handleConsoleInput(CmndInterp *me, char ch)
{
    if (me->state == 0) {
        if (ch == '/') me->state = 1;
        else gatherInputCharacters(me, ch);
    } else {                                    // '/' seen.
        if (ch == '\n') {                       // Not a command.
            gatherInputCharacters(me, '/');
            gatherInputCharacters(me, ch);
        } else {
            BSP_logf("\n");
            interpretCommand(me, ch);
        }
        me->state = 0;
    }
}


static void vLogToUser(CmndInterp *me, char const *fmt, va_list args)
{
    char msg[80];
    int nbw = vsnprintf(msg, sizeof msg, fmt, args);
    if (nbw > 0) DataLink_sendDebugPacket(me->datalink, (uint8_t const *)msg, nbw + 1);
}

/*
 * Below are the functions implementing this module's interface.
 */

void CLI_init(EventQueue *dq, DataLink *datalink)
{
    my.delegate_queue = dq;
    my.datalink = datalink;
}


int CLI_logf(char const *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int nbw = BSP_vlogf(fmt, args);
    vLogToUser(&my, fmt, args);
    va_end(args);
    return nbw;
}


void CLI_handleRemoteInput(uint8_t const *cmnd, uint16_t len)
{
    if (len == 2 && cmnd[0] == '/') {
        interpretCommand(&my, cmnd[1]);
    } else {
        char response[len + 5];
        int nc = snprintf(response, sizeof response, "'%.*s'?\n", len, cmnd);
        if (nc > 0 && nc < sizeof response) {
            DataLink_sendDebugPacket(my.datalink, (uint8_t const *)response, nc);
        }
    }
}


void BSP_idle(bool (*maySleep)(void const *), void const *context)
{
    char ch;
    if (BSP_readConsole(&ch, 1)) {
        handleConsoleInput(&my, ch);
    }

    if (maySleep != NULL) {
        BSP_criticalSectionEnter();
        if (maySleep(context)) {
            // All pending events have been handled.
            BSP_sleepMCU();
        }
        BSP_criticalSectionExit();
    }
}
