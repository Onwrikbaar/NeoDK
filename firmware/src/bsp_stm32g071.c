/*
 *  bsp_stm32g071.c
 *
 *  Created on: 20 Oct 2023
 *      Author: mark
 *   Copyright  2023 Neostim
 */

#include <limits.h>
#include <string.h>

#include "stm32g0xx_hal.h"
// #include "stm32g0xx_hal_rcc.h"
// #include "stm32g0xx_ll_pwr.h"
#include "stm32g0xx_ll_system.h"
#include "stm32g0xx_ll_gpio.h"
#include "stm32g0xx_ll_exti.h"
#include "stm32g0xx_ll_tim.h"
#include "stm32g0xx_ll_dac.h"
#include "stm32g0xx_ll_adc.h"

#include "bsp_dbg.h"

// This module implements:
#include "bsp_mao.h"
#include "bsp_app.h"


#define ADC_GPIO_PORT           GPIOA
#define TRF_CURR_SENSE_PIN      LL_GPIO_PIN_0   // ADC_IN0.
#define CAP_VOLT_SENSE_PIN      LL_GPIO_PIN_1   // ADC_IN1.
#define BAT_VOLT_SENSE_PIN      LL_GPIO_PIN_6   // ADC_IN6.

#define DAC_GPIO_PORT           GPIOA
#define VCAP_BUCK_CTRL_PIN      LL_GPIO_PIN_5   // DAC1_OUT2.

#define USART_GPIO_PORT         GPIOA
#define USART2_TX_PIN           LL_GPIO_PIN_2
#define USART2_RX_PIN           LL_GPIO_PIN_3

#define PUSHBUTTON_PORT         GPIOA
#define PUSHBUTTON_PIN          LL_GPIO_PIN_4   // Digital in with pulldown.

#define MOSFET_GPIO_PORT        GPIOA
#define MOSFET_1_PIN            LL_GPIO_PIN_8   // Digital out, push-pull.
#define MOSFET_2_PIN            LL_GPIO_PIN_9   // Digital out, push-pull.

#define TRIAC_GPIO_PORT         GPIOB
#define TRIAC_1_PIN             LL_GPIO_PIN_0   // Digital out, open drain.
#define TRIAC_2_PIN             LL_GPIO_PIN_1   // Digital out, open drain.
#define TRIAC_3_PIN             LL_GPIO_PIN_2   // Digital out, open drain.
#define TRIAC_4_PIN             LL_GPIO_PIN_5   // Digital out, open drain.
#define TRIAC_5_PIN             LL_GPIO_PIN_7   // Digital out, open drain.
#define TRIAC_6_PIN             LL_GPIO_PIN_8   // Digital out, open drain.

#define BUCK_GPIO_PORT          GPIOB
#define BUCK_ENABLE_PIN         LL_GPIO_PIN_9   // Digital out, push-pull.

#define LED_GPIO_PORT           GPIOC
#define LED_1_PIN               LL_GPIO_PIN_6   // The on-board LED.


#define ADC_TIMER_FREQ_Hz            100000UL
#define PULSE_TIMER_FREQ_Hz         1000000UL
#define APP_TIMER_FREQ_Hz           4000000UL
#define TICKS_PER_MICROSECOND       (APP_TIMER_FREQ_Hz / 1000000UL)


typedef struct {
    // The following members get set only once.
    void (*app_timer_handler)(uint64_t);
    uint32_t clock_ticks_per_app_timer_tick;
    Selector button_selector;
    Selector rx_sel;
    Selector rx_err_sel;
    void (*tx_callback)(void *, uint8_t *);
    void *tx_target;
    Selector tx_err_sel;
    // The following members get updated regularly.
    uint8_t critical_section_level;
    uint8_t nr_of_serial_devices;
    uint16_t volatile adc_1_samples[3];         // Must match the number of ADC1 ranks.
    uint16_t pulse_seqnr;
    uint8_t pulse_phase;
    uint8_t ci_state;
    uint8_t ci_len;
    uint8_t ci_buf[23];
} BSP;

// The interrupt request priorities, from high to low.
enum {  // The STM32G0xx has only four interrupt priority levels.
    IRQ_PRIO_SYSTICK, IRQ_PRIO_PULSE = IRQ_PRIO_SYSTICK,
    IRQ_PRIO_ADC_DMA,
    IRQ_PRIO_USART, IRQ_PRIO_APP_TIMER = IRQ_PRIO_USART,
    IRQ_PRIO_ADC1, IRQ_PRIO_EXTI = IRQ_PRIO_ADC1
};

// Ensure the following consts refer to the same timer.
static TIM_TypeDef *const pulse_timer = TIM1;   // Advanced 16-bit timer.
static IRQn_Type const pulse_timer_upd_irq = TIM1_BRK_UP_TRG_COM_IRQn;
static IRQn_Type const pulse_timer_cc_irq  = TIM1_CC_IRQn;

// Ensure the following two consts refer to the same timer.
static TIM_TypeDef *const app_timer = TIM2;     // General purpose 32-bit timer.
static IRQn_Type const app_timer_irq = TIM2_IRQn;

static BSP bsp = {0};


static void SystemClock_Config(void)
{
    HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);
    PWR->CR1 |= PWR_CR1_DBP;
    while ((PWR->CR1 & PWR_CR1_DBP) == RESET) { /* Wait for write protection to be removed. */ }
    RCC->BDCR = RCC_BDCR_LSEON | RCC_BDCR_LSEDRV_0 | RCC_BDCR_RTCEN;
    while ((RCC->BDCR & RCC_BDCR_LSERDY) == RESET) { /* Wait for LSE to stabilise. */ }
    // TODO Calibrate HSI using LSE and TIM16?

    RCC_OscInitTypeDef RCC_OscInitStruct = {
        .OscillatorType = RCC_OSCILLATORTYPE_HSI,
        .HSIState = RCC_HSI_ON,
        .HSIDiv = RCC_HSI_DIV1,
        .HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT,
        .PLL.PLLState = RCC_PLL_ON,
        .PLL.PLLSource = RCC_PLLSOURCE_HSI,
        .PLL.PLLM = RCC_PLLM_DIV4,
        .PLL.PLLN = 48,
        .PLL.PLLP = RCC_PLLP_DIV12,             // For 16 MHz clock.
        .PLL.PLLQ = RCC_PLLQ_DIV4,              // For 48 MHz clock.
        .PLL.PLLR = RCC_PLLR_DIV3               // For 64 MHz clock.
    };
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    RCC_ClkInitTypeDef RCC_ClkInitStruct = {
        .ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1,
        .SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK,
        .AHBCLKDivider = RCC_SYSCLK_DIV1,
        .APB1CLKDivider = RCC_HCLK_DIV1
    };
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2);

    PWR->CR1 &= ~PWR_CR1_DBP;
}


static void enableInterruptWithPrio(IRQn_Type intr, int prio)
{
    NVIC_ClearPendingIRQ(intr);
    NVIC_SetPriority(intr, prio);
    NVIC_EnableIRQ(intr);
}


static void spuriousIRQ(BSP *me)
{
    uint32_t const active_irq_nr = __get_IPSR();
    BSP_logf("%s(%u)\n", __func__, active_irq_nr);
    // TODO Disable the interrupt in question.
}


/*static*/ void powerOnSelfTest()
{
    // Configure all used pins as inputs.
    GPIOA->MODER = 0x28000000U;
    GPIOB->MODER = 0xfff00000U;
    GPIOC->MODER &= ~((3 << GPIO_MODER_MODE6_Pos) | (3 << GPIO_MODER_MODE14_Pos) | (3 << GPIO_MODER_MODE15_Pos));
    // Add pullups or pulldowns.
    GPIOA->PUPDR = (2 << GPIO_PUPDR_PUPD5_Pos) | (2 << GPIO_PUPDR_PUPD6_Pos) | (2 << GPIO_PUPDR_PUPD7_Pos)
                 | (2 << GPIO_PUPDR_PUPD10_Pos) | (2 << GPIO_PUPDR_PUPD12_Pos) | (2 << GPIO_PUPDR_PUPD15_Pos);
    GPIOB->PUPDR = (1 << GPIO_PUPDR_PUPD8_Pos) | (2 << GPIO_PUPDR_PUPD9_Pos);
    GPIOC->PUPDR = (2 << GPIO_PUPDR_PUPD2_Pos) | (1 << GPIO_PUPDR_PUPD6_Pos) | (1 << GPIO_PUPDR_PUPD14_Pos)
                 | (2 << GPIO_PUPDR_PUPD15_Pos);
    BSP_logf("%s:\n", __func__);
    BSP_logf("  Port A: 0x%04x\n", GPIOA->IDR);
    BSP_logf("  Port B: 0x%04x\n", GPIOB->IDR);
    BSP_logf("  Port C: 0x%04x\n", GPIOC->IDR);
}


static void initGPIO()
{
    LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Mode = LL_GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
    GPIO_InitStruct.Pin = TRF_CURR_SENSE_PIN | CAP_VOLT_SENSE_PIN | BAT_VOLT_SENSE_PIN;
    LL_GPIO_Init(ADC_GPIO_PORT, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = VCAP_BUCK_CTRL_PIN;
    LL_GPIO_Init(DAC_GPIO_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
    GPIO_InitStruct.Alternate = LL_GPIO_AF_1;
    GPIO_InitStruct.Pin = USART2_TX_PIN | USART2_RX_PIN;
    LL_GPIO_Init(USART_GPIO_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
    GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_MEDIUM;
    GPIO_InitStruct.Pin = BUCK_ENABLE_PIN;
    LL_GPIO_ResetOutputPin(BUCK_GPIO_PORT, GPIO_InitStruct.Pin);
    LL_GPIO_Init(BUCK_GPIO_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Mode  = LL_GPIO_MODE_ALTERNATE;
    GPIO_InitStruct.Alternate = LL_GPIO_AF_2;   // TIM1_CH1 and TIM1_CH2.
    // GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Pin = MOSFET_1_PIN | MOSFET_2_PIN;
    LL_GPIO_ResetOutputPin(MOSFET_GPIO_PORT, GPIO_InitStruct.Pin);
    LL_GPIO_Init(MOSFET_GPIO_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
    GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_OPENDRAIN;
    GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Pin = TRIAC_1_PIN | TRIAC_2_PIN | TRIAC_3_PIN | TRIAC_4_PIN;
    LL_GPIO_SetOutputPin(TRIAC_GPIO_PORT, GPIO_InitStruct.Pin);
    LL_GPIO_Init(TRIAC_GPIO_PORT, &GPIO_InitStruct);
}


static void initLEDandButton()
{
    LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin  = LED_1_PIN;
    GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
    GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_MEDIUM;
    LL_GPIO_SetOutputPin(LED_GPIO_PORT, GPIO_InitStruct.Pin);
    LL_GPIO_Init(LED_GPIO_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Mode = LL_GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = LL_GPIO_PULL_DOWN;
    GPIO_InitStruct.Pin  = PUSHBUTTON_PIN;
    LL_GPIO_Init(PUSHBUTTON_PORT, &GPIO_InitStruct);
}


static void initEXTI()
{
    // LL_EXTI_SetEXTISource(LL_EXTI_CONFIG_PORTA, LL_EXTI_CONFIG_LINE4);
    LL_EXTI_InitTypeDef EXTI_InitStruct = {
        .Line_0_31 = LL_EXTI_LINE_4,
        .LineCommand = ENABLE,
        .Mode = LL_EXTI_MODE_IT,
        .Trigger = LL_EXTI_TRIGGER_RISING_FALLING
    };
    LL_EXTI_Init(&EXTI_InitStruct);
}

/**
 * @brief Calculate the 12-bit value for the DAC to set the desired primary voltage [mV].
 */
static uint16_t Vcap_mV_ToDacVal(uint16_t Vcap_mV)
{
    if (Vcap_mV < 1065) Vcap_mV = 1065;
    else if (Vcap_mV > 10057) Vcap_mV = 10057;
    return (uint16_t)((233 * (uint32_t)(10057 - Vcap_mV)) / 512);
}


static void initDAC()
{
    LL_DAC_InitTypeDef DAC_InitStruct = {
        .TriggerSource = LL_DAC_TRIG_SOFTWARE,
        .WaveAutoGeneration = LL_DAC_WAVE_AUTO_GENERATION_NONE,
        .OutputBuffer = LL_DAC_OUTPUT_BUFFER_ENABLE,
        .OutputConnection = LL_DAC_OUTPUT_CONNECT_GPIO,
        .OutputMode = LL_DAC_OUTPUT_MODE_NORMAL
    };
    LL_DAC_Init(DAC1, LL_DAC_CHANNEL_2, &DAC_InitStruct);
    // TODO Wait for DAC to settle?
    LL_DAC_Enable(DAC1, LL_DAC_CHANNEL_2);
    // LL_DAC_EnableTrigger(DAC1, LL_DAC_CHANNEL_2);
    DAC1->DHR12R2 = Vcap_mV_ToDacVal(2000);     // Fixed voltage, for now.
}


static void initADC()
{
    BSP_logf("%s\n", __func__);
    LL_ADC_InitTypeDef gis = {
        // .Clock = LL_ADC_CLOCK_SYNC_PCLK_DIV4,
        .Resolution    = LL_ADC_RESOLUTION_12B,
        .DataAlignment = LL_ADC_DATA_ALIGN_RIGHT,
        .LowPowerMode  = LL_ADC_LP_MODE_NONE,
    };
    LL_ADC_Init(ADC1, &gis);
    LL_ADC_REG_SetSequencerConfigurable(ADC1, LL_ADC_REG_SEQ_CONFIGURABLE);
    while (LL_ADC_IsActiveFlag_CCRDY(ADC1) == 0) { /* Wait. */ }
    LL_ADC_ClearFlag_CCRDY(ADC1);

    LL_ADC_SetSamplingTimeCommonChannels(ADC1, LL_ADC_SAMPLINGTIME_COMMON_1, LL_ADC_SAMPLINGTIME_7CYCLES_5);
    LL_ADC_SetSamplingTimeCommonChannels(ADC1, LL_ADC_SAMPLINGTIME_COMMON_2, LL_ADC_SAMPLINGTIME_19CYCLES_5);

    // Configure a regular channel for the transformer's primary current sensing.
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_1, LL_ADC_CHANNEL_0);
    LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_0, LL_ADC_SAMPLINGTIME_COMMON_1);
    // Configure a regular channel for the capacitor voltage VCAP.
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_2, LL_ADC_CHANNEL_1);
    LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_1, LL_ADC_SAMPLINGTIME_COMMON_1);
    // Configure a regular channel for the battery/supply voltage.
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_3, LL_ADC_CHANNEL_6);
    LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_6, LL_ADC_SAMPLINGTIME_COMMON_1);

    LL_ADC_EnableInternalRegulator(ADC1);
    uint32_t volatile wli = LL_ADC_DELAY_INTERNAL_REGUL_STAB_US * (SystemCoreClock / 1000000);
    while (wli != 0) wli--;

    LL_ADC_StartCalibration(ADC1);
    while (LL_ADC_IsCalibrationOnGoing(ADC1)) { /* Wait. */ }
    ADC1->ISR = ADC_ISR_EOCAL;
    wli = LL_ADC_DELAY_CALIB_ENABLE_ADC_CYCLES * 16;
    while (wli != 0) wli--;

    // ADC->CCR |= ADC_CCR_VREFEN | ADC_CCR_TSEN;
    ADC1->CFGR2 |= ADC_CFGR2_LFTRIG;
    ADC1->IER = ADC_IER_ADRDYIE | ADC_IER_OVRIE;
    enableInterruptWithPrio(ADC1_COMP_IRQn, IRQ_PRIO_ADC1);

    LL_ADC_REG_InitTypeDef ris = {
        .SequencerLength  = LL_ADC_REG_SEQ_SCAN_ENABLE_3RANKS,
        .SequencerDiscont = LL_ADC_REG_SEQ_DISCONT_DISABLE,
        .ContinuousMode   = LL_ADC_REG_CONV_SINGLE,
        .TriggerSource    = LL_ADC_REG_TRIG_EXT_TIM3_TRGO,
        .DMATransfer      = LL_ADC_REG_DMA_TRANSFER_UNLIMITED,
        .Overrun          = LL_ADC_REG_OVR_DATA_OVERWRITTEN,
    };
    ErrorStatus es = LL_ADC_REG_Init(ADC1, &ris);
    if (es != SUCCESS) {
        BSP_logf("LL_ADC_REG_Init error=%u\n", es);
    }
    LL_ADC_SetOverSamplingScope(ADC1, LL_ADC_OVS_DISABLE);
    LL_ADC_SetTriggerFrequencyMode(ADC1, LL_ADC_CLOCK_FREQ_MODE_HIGH);
}


static void initDMAforADC(uint16_t volatile samples[], size_t nr_of_samples)
{
    LL_DMA_SetPeriphRequest(DMA1, LL_DMA_CHANNEL_1, LL_DMAMUX_REQ_ADC1);
    LL_DMA_ConfigTransfer(DMA1, LL_DMA_CHANNEL_1, LL_DMA_DIRECTION_PERIPH_TO_MEMORY | LL_DMA_MODE_CIRCULAR
                              | LL_DMA_PERIPH_NOINCREMENT | LL_DMA_MEMORY_INCREMENT
                              | LL_DMA_PDATAALIGN_HALFWORD | LL_DMA_MDATAALIGN_HALFWORD
                              | LL_DMA_PRIORITY_HIGH);
    LL_DMA_ConfigAddresses(DMA1, LL_DMA_CHANNEL_1, LL_ADC_DMA_GetRegAddr(ADC1, LL_ADC_DMA_REG_REGULAR_DATA),
                            (uint32_t)samples, LL_DMA_DIRECTION_PERIPH_TO_MEMORY);
    LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_1, nr_of_samples);
    LL_DMA_EnableIT_TC(DMA1, LL_DMA_CHANNEL_1); // Enable transfer complete interrupt.
    LL_DMA_EnableIT_TE(DMA1, LL_DMA_CHANNEL_1); // Enable transfer error interrupt.
    LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_1);

    DMA1->IFCR = ~0;                            // Clear all flags.
    enableInterruptWithPrio(DMA1_Channel1_IRQn, IRQ_PRIO_ADC_DMA);
}


static void initUSART2(uint32_t serial_speed_bps)
{
    RCC->CCIPR |= RCC_USART2CLKSOURCE_HSI;      // 16 MHz clock.
    USART2->BRR = (uint16_t)((16000000UL + serial_speed_bps / 2) / serial_speed_bps);
    // USART2->CR2 |= USART_CR2_SWAP;              // Temporary!
    USART2->CR1 |= USART_CR1_UE | USART_CR1_FIFOEN;
}


static void setPinVal(GPIO_TypeDef *GPIOx, uint16_t gpio_pin, uint8_t pin_state)
{
    if (pin_state != 0) {
        LL_GPIO_SetOutputPin(GPIOx, gpio_pin);
    } else {
        LL_GPIO_ResetOutputPin(GPIOx, gpio_pin);
    }
}


static void setSwitches(uint16_t pattern)
{
    // Turn on the LED if at least one triac will be activated.
    setPinVal(LED_GPIO_PORT, LED_1_PIN, pattern);

    // The triac enable signals are active low, so flip the bits.
    pattern ^= (uint16_t)0xffff;
    setPinVal(TRIAC_GPIO_PORT, TRIAC_1_PIN, pattern & 1);
    setPinVal(TRIAC_GPIO_PORT, TRIAC_2_PIN, pattern & 2);
    setPinVal(TRIAC_GPIO_PORT, TRIAC_3_PIN, pattern & 4);
    setPinVal(TRIAC_GPIO_PORT, TRIAC_4_PIN, pattern & 8);
}


static uint8_t switchesForConfig(uint16_t config_seqnr)
{
    static uint8_t sw_pat[] = {
        0x00,
        // 1: A - B | 2: A - C | 3: B - C | 4: AB - C | 5: B - AC
        0x03, 0x00, 0x09, 0x00, 0x0b,
        // 6: A - BC | 7: A - D | 8: B - D | 9: AB - D | 10: C - D
        0x00, 0x06, 0x00, 0x00, 0x0c,
        // 11: AC - D | 12: BC - D | 13: ABC - D | 14: B - AD | 15: C - AD
        0x0e, 0x00, 0x00, 0x00, 0x00,
        // 16: BC - AD | 17: A - BD | 18: C - BD | 19: AC - BD
        0x00, 0x07, 0x0d, 0x0f
    };

    return sw_pat[config_seqnr < M_DIM(sw_pat) ? config_seqnr : 0];
}


static void disableOutputStage()
{
    pulse_timer->CCER &= ~(TIM_CCER_CC1E | TIM_CCER_CC2E);
    LL_GPIO_ResetOutputPin(BUCK_GPIO_PORT, BUCK_ENABLE_PIN);
    LL_GPIO_SetOutputPin(TRIAC_GPIO_PORT, TRIAC_1_PIN | TRIAC_2_PIN | TRIAC_3_PIN | TRIAC_4_PIN);
}


static uint64_t ticksSinceBoot()
{
    static uint32_t ticks_since_boot[] = { 0UL, 0UL };

    BSP_criticalSectionEnter();                 // Protect our static var.
    uint32_t last_ticks = ticks_since_boot[0];  // Assuming little-Endian.
    if ((ticks_since_boot[0] = app_timer->CNT) < last_ticks) {
        ticks_since_boot[1] += 1;               // The 32-bit timer wrapped.
    }
    BSP_criticalSectionExit();
    return *(uint64_t *)ticks_since_boot;
}


static uint16_t pulsePaceMillisecondsToTicks(uint8_t pace_ms)
{
    uint32_t pace_ticks = (PULSE_TIMER_FREQ_Hz * pace_ms) / 1000;
    return pace_ticks <= 0xffff ? (uint16_t)pace_ticks : 0xffff;
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
            break;
        default:
            BSP_logf("%s(%u): unknown UART action\n", __func__, action);
    }
}


static void interpretCommand(BSP *me, char ch)
{
    switch (ch)
    {
        case '?':
            BSP_logf("Commands: /? /c /t\n");
            break;
        case 'c':
            BSP_logf("Turning on the Vcap buck\n");
            LL_GPIO_SetOutputPin(BUCK_GPIO_PORT, BUCK_ENABLE_PIN);
            break;
        case 't':                               // Toggle the LED.
            LL_GPIO_TogglePin(LED_GPIO_PORT, LED_1_PIN);
            break;
        default:
            BSP_logf("Unknown command '%c'\n", ch);
    }
}


static void gatherInputCharacters(BSP *me, char ch)
{
    me->ci_buf[me->ci_len] = '\0';
    if (ch == '\n' || (me->ci_buf[me->ci_len++] = ch, me->ci_len == sizeof me->ci_buf)) {
        if (ch != '\n') BSP_logf("\n");
        BSP_logf("Console: '%s'\n", me->ci_buf);
        me->ci_len = 0;
    }
}


static void handleConsoleInput(BSP *me, char ch)
{
    if (me->ci_state == 0) {
        if (ch == '/') me->ci_state = 1;
        else gatherInputCharacters(me, ch);
    } else {
        if (ch != '\n') BSP_logf("\n");
        interpretCommand(me, ch);
        me->ci_state = 0;
    }
}

/*
 * Following are the interrupt service routines.
 */

void HardFault_Handler(void)
{
    static volatile bool stay_here;

    disableOutputStage();
    BSP_logf("%s, ICSR=0x%x\n", __func__, SCB->ICSR);
    stay_here = true;
    BSP_logf("  Tip: halt CPU, change guard @ %p to 0, then step.\n", &stay_here);
    while (stay_here) {}
}

// Needed for HAL_Delay() to work.
void SysTick_Handler(void)
{
    HAL_IncTick();
}


void RCC_IRQHandler(void)
{
    // TODO Catch LSE ready interrupt?
    spuriousIRQ(&bsp);
}


void TIM1_BRK_UP_TRG_COM_IRQHandler(void)
{
    if (pulse_timer->SR & TIM_SR_UIF) {         // An update event.
        pulse_timer->CR1 &= ~TIM_CR1_CEN;       // Stop the counter.
        pulse_timer->DIER &= ~(TIM_DIER_CC1IE | TIM_DIER_CC2IE);
        pulse_timer->SR &= ~(TIM_SR_UIF | TIM_SR_CC1IF | TIM_SR_CC2IF);
        setSwitches(0);                         // Disconnect from the load.
        BSP_logf("Pulse timer update\n");
        // handlePulseUpdateEvent(&bsp);
    } else {
        BSP_logf("Pt SR=0x%x\n", __func__, pulse_timer->SR);
        // spuriousIRQ(&bsp);
    }
}


void TIM1_CC_IRQHandler(void)
{
    bsp.pulse_seqnr += 1;
    if ((pulse_timer->DIER & TIM_DIER_CC1IE) && (pulse_timer->SR & TIM_SR_CC1IF)) {
        pulse_timer->SR &= ~TIM_SR_CC1IF;
        BSP_logf("CC1 %hu\n", bsp.pulse_seqnr);
    }
    if ((pulse_timer->DIER & TIM_DIER_CC2IE) && (pulse_timer->SR & TIM_SR_CC2IF)) {
        pulse_timer->SR &= ~TIM_SR_CC2IF;
        BSP_logf("CC2 %hu\n", bsp.pulse_seqnr);
    }
    if (bsp.pulse_seqnr == pulse_timer->RCR + 1) {
        BSP_logf("Last pulse done\n");
        LL_GPIO_ResetOutputPin(LED_GPIO_PORT, LED_1_PIN);
        // TODO Signal the Sequencer.
    }
    if (pulse_timer->SR & 0xcffe0) {
        BSP_logf("Pt SR=0x%x\n", pulse_timer->SR & 0xcffe0);
        // spuriousIRQ(&bsp);
    }
}


void TIM2_IRQHandler(void)
{
    if (app_timer->SR & TIM_SR_CC1IF) {         // Capture/compare 1.
        app_timer->SR &= ~TIM_SR_CC1IF;         // Clear the interrupt.
        bsp.app_timer_handler(BSP_microsecondsSinceBoot());
        app_timer->CCR1 += bsp.clock_ticks_per_app_timer_tick;
        return;
    }
    if (app_timer->SR & TIM_SR_UIF) {
        app_timer->SR &= ~TIM_SR_UIF;           // Clear the interrupt.
        // TODO ?
    } else {
        spuriousIRQ(&bsp);
    }
}


void EXTI4_15_IRQHandler(void)
{
    if (EXTI->RPR1 & PUSHBUTTON_PIN) {
        EXTI->RPR1 = PUSHBUTTON_PIN;            // Rising.
        invokeSelector(&bsp.button_selector, 1);
    } else if (EXTI->FPR1 & PUSHBUTTON_PIN) {
        EXTI->FPR1 = PUSHBUTTON_PIN;            // Falling.
        invokeSelector(&bsp.button_selector, 0);
    } else {
        spuriousIRQ(&bsp);
    }
}


void TIM6_DAC_LPTIM1_IRQHandler(void)
{
    // TODO Check for DAC interrupt.
    spuriousIRQ(&bsp);
}


void ADC1_COMP_IRQHandler(void)
{
    if (ADC1->ISR & ADC_ISR_OVR) {              // Overrun?
        ADC1->ISR = ADC_ISR_OVR;                // Clear.
        BSP_logf("ADC overrun\n");
    } else if (ADC1->ISR & ADC_ISR_ADRDY) {
        ADC1->ISR = ADC_ISR_ADRDY;              // Clear.
        BSP_logf("ADC ready\n");
    } else if (ADC1->ISR & ADC_ISR_CCRDY) {
        ADC1->ISR = ADC_ISR_CCRDY;              // Clear.
        BSP_logf("ADC channel config ready\n");
    } else if (ADC1->ISR & ADC_ISR_EOC) {       // Debugging only.
        ADC1->ISR = ADC_ISR_EOC;                // Clear.
        BSP_logf("ADC EOC\n");
    } else if (ADC1->ISR & ADC_ISR_EOS) {       // Debugging only.
        ADC1->ISR = ADC_ISR_EOS;                // Clear.
        BSP_logf("ADC EOS\n");
    } else {
        BSP_logf("ADC1->ISR=0x%x\n", ADC1->ISR);
        spuriousIRQ(&bsp);
    }
}


void DMA1_Channel1_IRQHandler(void)
{
    if (DMA1->ISR & DMA_ISR_TCIF1) {
        DMA1->IFCR = DMA_IFCR_CTCIF1;           // Clear transfer complete flag.
        // TODO Process the ADC values.
    } else if (DMA1->ISR & DMA_ISR_TEIF1) {
        DMA1->IFCR = DMA_IFCR_CTEIF1;           // Clear transfer error flag.
        BSP_logf("%s, TEIF1\n", __func__);
        LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_1);
    } else {
        BSP_logf("%s, ISR=0x%x\n", __func__, DMA1->ISR);
        DMA1->IFCR = ~0;
    }
}


void USART2_IRQHandler(void)
{
    if (USART2->ISR & USART_ISR_RXNE_RXFNE) {   // Byte received.
        invokeSelector(&bsp.rx_sel, USART2->RDR);
    } else if (USART2->ISR & USART_ISR_TXE_TXFNF) {
        uint8_t b;
        bsp.tx_callback(bsp.tx_target, &b);     // Get one byte from the source,
        USART2->TDR = b;                        // and write it to the serial port.
    } else {
        spuriousIRQ(&bsp);
    }
}

/*
 * Support for STM32Cube LL drivers.
 */

void assert_failed(uint8_t *filename, uint32_t line_nr)
{
    disableOutputStage();
    BSP_logf("%s(%s, %u)\n", __func__, filename, line_nr);
    // TODO Register the problem and then reboot.
    while (1) {}
}

/*
 * Below are the functions implementing this module's interface.
 */

void BSP_init()
{
    LL_RCC_SetADCClockSource(LL_RCC_ADC_CLKSOURCE_HSI);
    RCC->APBENR1 = RCC_APBENR1_TIM2EN | RCC_APBENR1_TIM3EN | RCC_APBENR1_USART2EN | RCC_APBENR1_PWREN | RCC_APBENR1_DAC1EN;
    RCC->APBENR2 = RCC_APBENR2_TIM1EN | RCC_APBENR2_TIM16EN | RCC_APBENR2_ADCEN | RCC_APBENR2_SYSCFGEN;
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;
    RCC->IOPENR |= RCC_IOPENR_GPIOAEN | RCC_IOPENR_GPIOBEN | RCC_IOPENR_GPIOCEN;
    // powerOnSelfTest();                          // Check whether the electronics are correct.
    // Enable instruction cache and prefetch buffer.
    FLASH->ACR |= FLASH_ACR_ICEN | FLASH_ACR_PRFTEN;

    HAL_InitTick(IRQ_PRIO_SYSTICK);
    SystemClock_Config();
    BSP_logf("DevId=0x%x, SystemCoreClock=%u, NVIC_PRIO_BITS=%u\n", LL_DBGMCU_GetDeviceID(), SystemCoreClock, __NVIC_PRIO_BITS);
    initGPIO();
    initLEDandButton();
    initEXTI();
    initDAC();
    initDMAforADC(bsp.adc_1_samples, M_DIM(bsp.adc_1_samples));
    initADC();
    LL_ADC_Enable(ADC1);

    bsp.nr_of_serial_devices = 1;               // Device 0 is the SEGGER console.
}


void BSP_initComms(void)
{
    initUSART2(57600UL);
}


void BSP_criticalSectionEnter()
{
    __disable_irq();
    __DSB();
    __ISB();
    M_ASSERT(bsp.critical_section_level != UCHAR_MAX);
    bsp.critical_section_level += 1;
}


void BSP_criticalSectionExit()
{
    M_ASSERT(bsp.critical_section_level != 0);
    if (--bsp.critical_section_level == 0) {
        __DSB();
        __ISB();
        __enable_irq();
    }
}


uint32_t BSP_millisecondsToTicks(uint16_t ms)
{
    uint32_t const ticks_per_millisecond = APP_TIMER_FREQ_Hz / 1000UL;
    return (uint32_t)ms * ticks_per_millisecond;
}


uint64_t BSP_ticksToMicroseconds(uint64_t ticks)
{
    return ticks / TICKS_PER_MICROSECOND;
}


uint64_t BSP_microsecondsSinceBoot()
{
    return BSP_ticksToMicroseconds(ticksSinceBoot());
}


void BSP_registerAppTimerHandler(void (*handler)(uint64_t), uint32_t microseconds_per_app_timer_tick)
{
    BSP_logf("%s\n", __func__);
    app_timer->PSC = SystemCoreClock / APP_TIMER_FREQ_Hz - 1;

    app_timer->CCR1 = BSP_millisecondsToTicks(100);
    app_timer->CCMR1 = TIM_CCMR1_OC1M_0;        // Set channel 1 to active level on match.
    app_timer->CCER |= TIM_CCER_CC1E;           // Compare 1 output enable.
    app_timer->DIER |= TIM_DIER_CC1IE;          // Interrupt on match with compare register.
    bsp.app_timer_handler = handler;
    bsp.clock_ticks_per_app_timer_tick = microseconds_per_app_timer_tick * TICKS_PER_MICROSECOND;

    app_timer->EGR |= (TIM_EGR_UG | TIM_EGR_CC1G);
    app_timer->CR1 = TIM_CR1_CEN;               // Enable counter.
    enableInterruptWithPrio(app_timer_irq, IRQ_PRIO_APP_TIMER);
}


void BSP_registerPulseHandler(void *pulse_ao)
{
    pulse_timer->PSC = SystemCoreClock / PULSE_TIMER_FREQ_Hz - 1;
    BSP_logf("%s: PSC=%u, ARR=%u\n", __func__, pulse_timer->PSC, pulse_timer->ARR);
    // PWM mode 1 for both channels, enable preload.
    pulse_timer->CCMR1 = TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1PE
                       | TIM_CCMR1_OC2M_2 | TIM_CCMR1_OC2M_1 | TIM_CCMR1_OC2PE;
    // Enable both outputs.
    pulse_timer->CCER = TIM_CCER_CC1E | TIM_CCER_CC2E;
    pulse_timer->CR2 = TIM_CR2_CCPC;            // Compare control signals are preloaded.
    enableInterruptWithPrio(pulse_timer_upd_irq, IRQ_PRIO_PULSE);
    enableInterruptWithPrio(pulse_timer_cc_irq,  IRQ_PRIO_PULSE);
    pulse_timer->CR1 = TIM_CR1_ARPE | TIM_CR1_URS;
    pulse_timer->DIER = TIM_DIER_UIE;           // Enable update interrupt.
    pulse_timer->BDTR = TIM_BDTR_OSSR | TIM_BDTR_MOE;
}


void BSP_registerButtonHandler(Selector *sel)
{
    bsp.button_selector = *sel;
    enableInterruptWithPrio(EXTI4_15_IRQn, IRQ_PRIO_EXTI);
}


void BSP_idle(bool (*maySleep)(void))
{
    if (maySleep == NULL || maySleep()) {       // All pending events have been handled.
        char ch;
        if (BSP_readConsole(&ch, 1)) {
            handleConsoleInput(&bsp, ch);
        }
        // TODO Put the processor to sleep?
    }
}


DeviceId BSP_openSerialPort(char const *name)
{
    if (bsp.nr_of_serial_devices >= 2) {
        BSP_logf("Can only use one serial port at a time, for now\n");
        return -1;
    }

    enableInterruptWithPrio(USART2_IRQn, IRQ_PRIO_USART);
    return bsp.nr_of_serial_devices++;
}


void BSP_registerRxCallback(DeviceId device_id, Selector const *rx_sel, Selector const *rx_err_sel)
{
    M_ASSERT(rx_sel != NULL && rx_err_sel != NULL);
    if (device_id == 1) {
        bsp.rx_sel = *rx_sel;
        bsp.rx_err_sel = *rx_err_sel;
        USART2->CR1 |= USART_CR1_RE;            // Enable the receiver.
    }
}


void BSP_registerTxCallback(DeviceId device_id, void (*cb)(void *, uint8_t *), void *target, Selector const *tx_err_sel)
{
    M_ASSERT(cb != NULL && target != NULL);
    if (device_id == 1) {
        bsp.tx_target = target;
        bsp.tx_callback = cb;
        bsp.tx_err_sel = *tx_err_sel;
        USART2->CR1 |= USART_CR1_TE;            // Enable the transmitter.
    }
}


void BSP_toggleLED()
{
    LL_GPIO_TogglePin(LED_GPIO_PORT, LED_1_PIN);
}


void BSP_doChannelAction(DeviceId device_id, ChannelAction action)
{
    // BSP_logf("%s(%u, %u)\n", __func__, device_id, action);
    switch (device_id)
    {
        case 1:
            if (device_id >= 0) doUartAction(USART2, action);
            break;
        default:
            BSP_logf("%s(%u): unknown device id\n", __func__, device_id);
    }
}


int BSP_closeSerialPort(int device_id)
{
    M_ASSERT(device_id == bsp.nr_of_serial_devices - 1);
    // TODO De-initialise USART2.
    NVIC_DisableIRQ(USART2_IRQn);
    return 0;
}


bool BSP_selectElectrodeConfiguration(uint16_t config_seqnr)
{
    uint8_t sw_pat = switchesForConfig(config_seqnr);
    if (sw_pat == 0x00) return false;

    setSwitches(sw_pat);
    return true;
}


bool BSP_startPulseTrain(uint8_t phase, uint8_t pace_ms, uint8_t pulse_width_micros, uint16_t nr_of_pulses)
{
    M_ASSERT(phase <= 1);
    M_ASSERT(pace_ms >= 4);                     // Repetition rate <= 250 Hz.
    M_ASSERT(pulse_width_micros != 0);
    M_ASSERT(nr_of_pulses != 0);

    pulse_timer->ARR = pulsePaceMillisecondsToTicks(pace_ms) - 1;
    pulse_timer->RCR = nr_of_pulses - 1;
    pulse_timer->CNT = 0;
    pulse_timer->SR &= ~(TIM_SR_CC1IF | TIM_SR_CC2IF);
    if (phase == 0) {
        pulse_timer->CCR1 = pulse_width_micros - 1;
        pulse_timer->DIER |= TIM_DIER_CC1IE;
    } else {
        pulse_timer->CCR2 = pulse_width_micros - 1;
        pulse_timer->DIER |= TIM_DIER_CC2IE;
    }
    bsp.pulse_seqnr = 0;
    pulse_timer->EGR |= TIM_EGR_UG;             // Force update of the shadow registers.
    pulse_timer->CR1 |= TIM_CR1_CEN;            // Enable the counter.
    // Insert a tiny delay here if the CCRs get cleared before the update takes effect.
    pulse_timer->CCR1 = 0;
    pulse_timer->CCR2 = 0;
    return true;
}


void BSP_close()
{
    __disable_irq();
}


void BSP_shutDown()
{
    // TODO Turn off the processor.
    while (1) {}
}
