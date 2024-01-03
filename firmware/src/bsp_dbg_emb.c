/*
 * bsp_dbg_emb.c -- debugging functions for embedded platforms.
 *
 *  Created on: 27 Mar 2021
 *      Author: mark
 *   Copyright  2021..2024 Neostim
 */

#include <stdarg.h>

#include "SEGGER_RTT.h"

// This module implements:
#include "bsp_dbg.h"

// Symbol not declared in any header file.
extern int SEGGER_RTT_vprintf(unsigned int, char const *, va_list *);

/*
 * Below are the functions implementing this module's interface.
 */

void BSP_initDebug()
{
    SEGGER_RTT_Init();
}


int BSP_vlogf(char const *fmt, va_list args)
{
    return SEGGER_RTT_vprintf(0, fmt, &args);
}


int BSP_logf(char const *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int nbw = BSP_vlogf(fmt, args);
    va_end(args);
    return nbw;
}


void BSP_assertionFailed(char const *filename, unsigned int line_number, char const *predicate)
{
    BSP_logf("ASSERTION FAILED (%s : %u) %s\n", filename, line_number, predicate);
    BSP_closeDebug();                           // Clean up a bit.
    while (1) {}                                // Stay here forever.
}


bool BSP_getKey(char *pch)
{
    int gk = SEGGER_RTT_GetKey();
    if (gk >= 0) {
        *pch = (char)gk;
        return true;
    }
    return false;
}

// Get characters from the console, nonblocking.
int BSP_readConsole(char *cbuf, int nr_of_chars)
{
    int nb = SEGGER_RTT_Read(0, cbuf, nr_of_chars);
    return nb < 0 ? 0 : nb;
}


void BSP_closeDebug()
{
    // Nothing to do yet.
}
