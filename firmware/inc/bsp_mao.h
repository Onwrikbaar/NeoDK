/*
 * bsp_mao.h
 *
 *  Created on: 23 May 2021
 *      Author: mark
 *   Copyright  2021..2023 Neostim
 */

#ifndef INC_BSP_MAO_H_
#define INC_BSP_MAO_H_

#include "convenience.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CA_NONE,
    CA_RX_CB_ENABLE, CA_RX_CB_DISABLE,
    CA_TX_CB_ENABLE, CA_TX_CB_DISABLE,
    CA_OVERRUN_CB_ENABLE, CA_OVERRUN_CB_DISABLE,
    CA_FRAMING_CB_ENABLE, CA_FRAMING_CB_DISABLE,
    CA_NOISE_CB_ENABLE, CA_NOISE_CB_DISABLE,
    CA_CLEAR_ERRORS, CA_CLOSE
} ChannelAction;

typedef enum {
    CE_NONE,
    CE_RX_OVERRUN,
    CE_RX_FRAMING,
    CE_RX_NOISE,
    CE_RX_READ_FAILED,
    CE_TX_WRITE_FAILED,
} ChannelError;

/*
 * These are the platform-dependent functions used by Maolib.
 * All Maolib clients must implement them.
 */
void BSP_criticalSectionEnter();
void BSP_criticalSectionExit();
void BSP_registerAppTimerHandler(void (*)(uint64_t), uint32_t microseconds_per_app_timer_tick);
uint64_t BSP_microsecondsSinceBoot();

/*
 * Implement these functions in the application's
 * BSP module when using comms channels.
 */
void BSP_registerRxCallback(DeviceId, Selector const *rx_sel, Selector const *rx_err_sel);
void BSP_registerTxCallback(DeviceId, void (*)(void *, uint8_t *), void *, Selector const *tx_err_sel);
void BSP_doChannelAction(DeviceId, ChannelAction);

// Implementing this function is optional.
void BSP_idle(bool (*maySleep)(void));

#ifdef __cplusplus
}
#endif

#endif
