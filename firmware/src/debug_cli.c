/*
 * debug_cli.c
 *
 *  Created on: 26 Mar 2024
 *      Author: mark
 *   Copyright  2024 Neostim
 */

#include "bsp_dbg.h"
#include "bsp_app.h"

// This module implements:
#include "debug_cli.h"


typedef struct {
    uint8_t buf[30];
    uint8_t len;
    uint8_t state;
    Comms *comms;
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
            BSP_logf("Commands: /? /l /s /0 /1../9\n");
            break;
        case '0':
            BSP_primaryVoltageEnable(false);
            BSP_setPrimaryVoltage_mV(0);
            break;
        case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
            BSP_primaryVoltageEnable(true);
            BSP_setPrimaryVoltage_mV((ch - '0') * 1000U);
            break;
        case 'l':
            BSP_toggleTheLED();
            break;
        case 's':
            Comms_waitForSync(me->comms);
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

void CLI_init(Comms *comms)
{
    my.comms = comms;
}


void BSP_idle(bool (*maySleep)(void))
{
    char ch;
    if (BSP_readConsole(&ch, 1)) {
        handleConsoleInput(&my, ch);
    }

    if (maySleep == NULL || maySleep()) {       // All pending events have been handled.
        // TODO Put the processor to sleep to save power?
    }
}
