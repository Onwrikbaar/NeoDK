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

#include "bsp_dbg.h"
#include "bsp_mao.h"
#include "bsp_app.h"
#include "app_event.h"

// This module implements:
#include "debug_cli.h"


typedef struct {
    uint8_t buf[30];
    uint8_t len;
    uint8_t state;
    EventQueue *delegate_queue;
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

/**
 * @brief   Console commands for interactive testing.
 */
static void interpretCommand(CmndInterp *me, char ch)
{
    switch (ch)
    {
        case '?':
            BSP_logf("Commands: /? /a /l /q /s /0 /1../9\n");
            break;
        case '0':
            BSP_primaryVoltageEnable(false);
            BSP_setPrimaryVoltage_mV(0);
            break;
        case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
            BSP_primaryVoltageEnable(true);
            BSP_setPrimaryVoltage_mV((ch - '0') * 1000U);
            break;
        case 'a':
            BSP_triggerADC();
            break;
        case 'l':
            BSP_toggleTheLED();
            break;
        case 'q': {                             // Quit.
            int sig = 2;                        // Simulate Ctrl-C.
            EventQueue_postEvent(me->delegate_queue, ET_POSIX_SIGNAL, (uint8_t const *)&sig, sizeof sig);
            break;
        }
        case 's':
            EventQueue_postEvent(me->delegate_queue, ET_COMMS_WAIT_FOR_SYNC, NULL, 0);
            break;
        default:
            BSP_logf("Unknown command '/%c'\n", ch);
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

/*
 * Below are the functions implementing this module's interface.
 */

void CLI_init(EventQueue *dq)
{
    my.delegate_queue = dq;
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
