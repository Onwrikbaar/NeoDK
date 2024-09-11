/*
 * controller.c
 *
 *  NOTICE (do not remove):
 *      This file is part of project NeoDK (https://github.com/Onwrikbaar/NeoDK).
 *      See https://github.com/Onwrikbaar/NeoDK/blob/main/LICENSE.txt for full license details.
 *
 *  Created on: 21 Aug 2024
 *      Author: mark
 *   Copyright  2024 Neostim™
 */

#include <stdlib.h>
#include <string.h>

#include "bsp_dbg.h"
#include "debug_cli.h"

// This module implements:
#include "controller.h"


struct _Controller {
    DataLink *datalink;
};


static void handleHostMessage(Controller *me, uint8_t const *msg, uint16_t nb)
{
    char nt_msg[nb + 1];
    strncpy(nt_msg, (const char *)msg, nb);
    nt_msg[nb] = '\0';
    CLI_handleConsoleInput(nt_msg, nb);
}

/*
 * Below are the functions implementing this module's interface.
 */

Controller *Controller_new()
{
    return (Controller *)malloc(sizeof(Controller));
}


void Controller_init(Controller *me, DataLink *datalink)
{
    me->datalink = datalink;
    BSP_logf("%s\n", __func__);
    DataLink_open(me->datalink, me, (PacketCallback)&handleHostMessage);
    DataLink_waitForSync(me->datalink);
}


void Controller_stop(Controller *me)
{
    BSP_logf("%s\n", __func__);
    DataLink_close(me->datalink);
}


void Controller_delete(Controller *me)
{
    free(me);
}
