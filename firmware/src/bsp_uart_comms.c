/*
 *  bsp_uart_comms.c
 *
 *  NOTICE (do not remove):
 *      This file is part of project NeoDK (https://github.com/Onwrikbaar/NeoDK).
 *      See https://github.com/Onwrikbaar/NeoDK/blob/main/LICENSE.txt for full license details.
 *
 *  Created on: 5 Apr 2025
 *      Author: mark
 *   Copyright  2025 Neostim™
 */

#include "stm32g071xx.h"
#include "stm32g0xx_ll_gpio.h"
#include "stm32g0xx_ll_rcc.h"

#include "bsp_dbg.h"

// This module implements (parts of):
#include "bsp_mao.h"
#include "bsp_comms.h"


#define USART_GPIO_PORT         GPIOA
#define USART2_TX_PIN           LL_GPIO_PIN_2
#define USART2_RX_PIN           LL_GPIO_PIN_3

typedef struct {
    // The following members get set only once.
    Selector rx_sel;
    Selector rx_err_sel;
    void (*tx_callback)(void *, uint8_t *);
    void *tx_target;
    Selector tx_err_sel;
    // The following members may get updated regularly.
    uint8_t nr_of_serial_devices;
} Comms;


static Comms com = {0};


static void initUsartGpio(void)
{
    LL_GPIO_InitTypeDef GPIO_InitStruct = {
        .Mode = LL_GPIO_MODE_ALTERNATE,
        .Alternate = LL_GPIO_AF_1,
        .Pin = USART2_TX_PIN | USART2_RX_PIN
    };
    LL_GPIO_Init(USART_GPIO_PORT, &GPIO_InitStruct);
}


static void initUSART2(uint32_t serial_speed_bps)
{
    USART2->BRR = (uint16_t)((HSI_VALUE + serial_speed_bps / 2) / serial_speed_bps);
    USART2->CR1 |= USART_CR1_UE | USART_CR1_FIFOEN;
}


static void doUartAction(USART_TypeDef *uart, ChannelAction action)
{
    switch (action)
    {
        case CA_RX_CB_ENABLE:
            uart->CR1 |= USART_CR1_RXNEIE_RXFNEIE;
            break;
        case CA_RX_CB_DISABLE:
            uart->CR1 &= ~USART_CR1_RXNEIE_RXFNEIE;
            break;
        case CA_TX_CB_ENABLE:
            uart->CR1 |= USART_CR1_TXEIE_TXFNFIE;
            break;
        case CA_TX_CB_DISABLE:
            uart->CR1 &= ~USART_CR1_TXEIE_TXFNFIE;
            break;
        case CA_OVERRUN_CB_ENABLE:
            break;
        case CA_OVERRUN_CB_DISABLE:
            break;
        case CA_FRAMING_CB_ENABLE:
            break;
        case CA_FRAMING_CB_DISABLE:
            break;
        case CA_CLEAR_ERRORS:
            uart->ICR = USART_ICR_ORECF | USART_ICR_PECF | USART_ICR_NECF | USART_ICR_UDRCF;
            break;
        case CA_CLOSE:
            // TODO Deinit the UART?
            break;
        default:
            BSP_logf("%s(%u): unknown UART action\n", __func__, action);
    }
}

/*
 * Following are the interrupt service routines.
 */

void USART2_IRQHandler(void)
{
    if (USART2->ISR & USART_ISR_RXNE_RXFNE) {
        invokeSelector(&com.rx_sel, USART2->RDR);
    } else if (USART2->ISR & USART_ISR_TXE_TXFNF) {
        if (USART2->CR1 & USART_CR1_TXEIE_TXFNFIE) {
            com.tx_callback(com.tx_target, (uint8_t *)&USART2->TDR);
        }
    } else {
        BSP_logf("%s\n", __func__);
    }
}

/*
 * Below are the functions implementing this module's interface.
 */

void BSP_initComms(void)
{
    RCC->APBENR1 |= RCC_APBENR1_USART2EN;
    RCC->CCIPR |= LL_RCC_USART2_CLKSOURCE_HSI;
    com.nr_of_serial_devices = 1;               // Device 0 is the SEGGER console.
    initUsartGpio();
    initUSART2(115200UL);
}


DeviceId BSP_openSerialPort(char const *name)
{
    if (com.nr_of_serial_devices > 1) {
        BSP_logf("Can only use one serial port at a time, for now\n");
        return -1;
    }

    BSP_enableUartInterrupt(USART2_IRQn);
    return com.nr_of_serial_devices++;
}


void BSP_registerRxCallback(DeviceId device_id, Selector const *rx_sel, Selector const *rx_err_sel)
{
    M_ASSERT(rx_sel != NULL && rx_err_sel != NULL);
    if (device_id == 1) {
        com.rx_sel = *rx_sel;
        com.rx_err_sel = *rx_err_sel;
        USART2->CR1 |= USART_CR1_RE;            // Enable the receiver.
    }
}


void BSP_registerTxCallback(DeviceId device_id, void (*cb)(void *, uint8_t *), void *target, Selector const *tx_err_sel)
{
    M_ASSERT(cb != NULL && target != NULL);
    if (device_id == 1) {
        com.tx_target = target;
        com.tx_callback = cb;
        com.tx_err_sel = *tx_err_sel;
        USART2->CR1 |= USART_CR1_TE;            // Enable the transmitter.
    }
}


void BSP_doChannelAction(DeviceId device_id, ChannelAction action)
{
    // BSP_logf("%s(%u, %u)\n", __func__, device_id, action);
    switch (device_id)
    {
        case 1:
            doUartAction(USART2, action);
            break;
        default:
            BSP_logf("%s(%u): unknown device id\n", __func__, device_id);
    }
}


int BSP_closeSerialPort(int device_id)
{
    M_ASSERT(device_id == com.nr_of_serial_devices - 1);
    // TODO De-initialise USART2?
    NVIC_DisableIRQ(USART2_IRQn);
    return 0;
}
