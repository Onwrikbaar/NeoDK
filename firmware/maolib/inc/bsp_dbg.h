/*
 * bsp_dbg.h -- the platform-dependent debugging functions.
 *
 *  Created on: 27 Mar 2021
 *      Author: mark
 *   Copyright  2021..2025 Neostim™
 */

#ifndef INC_BSP_DBG_H_
#define INC_BSP_DBG_H_

#include <stdbool.h>
#include <stdarg.h>

#ifndef M_ASSERT
#define M_ASSERT(predicate)     if (!(predicate)) BSP_assertionFailed(__FILE__, __LINE__, #predicate)
#endif

#ifdef __cplusplus
extern "C" {
#endif

void BSP_initDebug(void);
int  BSP_logf(char const *fmt, ...);            // Print logging and debugging information.
int  BSP_vlogf(char const *fmt, va_list args);
void BSP_assertionFailed(char const *filename, unsigned int line_number, char const *predicate);
bool BSP_getKey(char *pch);
int  BSP_readConsole(char *cbuf, int nr_of_chars);
void BSP_closeDebug(void);

#ifdef __cplusplus
}
#endif

#endif
