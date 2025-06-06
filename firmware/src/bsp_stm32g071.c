/*
 *  bsp_stm32g071.c
 *
 *  NOTICE (do not remove):
 *      This file is part of project NeoDK (https://github.com/Onwrikbaar/NeoDK).
 *      See https://github.com/Onwrikbaar/NeoDK/blob/main/LICENSE.txt for full license details.
 *
 *  Created on: 20 Oct 2023
 *      Author: mark
 *   Copyright  2023..2025 Neostim™
 */

#include <limits.h>
#include <string.h>

#include "stm32g0xx_hal_rcc.h"
// #include "stm32g0xx_ll_rcc.h"
// #include "stm32g0xx_ll_pwr.h"
#include "stm32g0xx_hal_pwr.h"
// #include "stm32g0xx_hal_pwr_ex.h"
#include "stm32g0xx_ll_system.h"
#include "stm32g0xx_ll_gpio.h"
#include "stm32g0xx_ll_exti.h"
#include "stm32g0xx_ll_tim.h"
#include "stm32g0xx_ll_dac.h"
#include "stm32g0xx_ll_adc.h"
#include "stm32g0xx_ll_dma.h"

#include "bsp_dbg.h"
#include "app_event.h"

// This module implements:
#include "bsp_mao.h"
#include "bsp_app.h"


#define ADC_GPIO_PORT           GPIOA
#define TRF_CURR_SENSE_PIN      LL_GPIO_PIN_0   // ADC_IN0.
#define CAP_VOLT_SENSE_PIN      LL_GPIO_PIN_1   // ADC_IN1.
#define BAT_VOLT_SENSE_PIN      LL_GPIO_PIN_6   // ADC_IN6.

#define DAC_GPIO_PORT           GPIOA
#define VCAP_BUCK_CTRL_PIN      LL_GPIO_PIN_5   // DAC1_OUT2.

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
#define ALL_TRIAC_PINS          (TRIAC_1_PIN | TRIAC_2_PIN | TRIAC_3_PIN | TRIAC_4_PIN)

#define BUCK_GPIO_PORT          GPIOB
#define BUCK_ENABLE_PIN         LL_GPIO_PIN_9   // Digital out, push-pull.

#define LED_GPIO_PORT           GPIOC
#define LED_1_PIN               LL_GPIO_PIN_6   // The on-board LED.

#define ALL_TRIACS_OFF          0x00

// #define ADC_TIMER_FREQ_Hz        100000UL
#define SEQUENCER_CLOCK_FREQ_Hz 1000000UL
#define PULSE_TIMER_FREQ_Hz     1000000UL
#define APP_TIMER_FREQ_Hz       2000000UL
#define TICKS_PER_MICROSECOND   (APP_TIMER_FREQ_Hz / 1000000UL)


typedef struct {
    // The following members get set only once.
    void (*app_timer_handler)(void *, uint64_t);
    void *app_timer_target;
    uint32_t clock_ticks_per_app_timer_tick;
    Selector button_sel;
    EventQueue *delegate;
    // The following members may get updated regularly.
    uint8_t critical_section_level;
    uint16_t volatile adc_1_samples[3];         // Must match the number of ADC1 ranks.
    uint16_t V_prim_mV;
    uint16_t volatile pulse_seqnr;
    Burst volatile next_burst;
    // Deltas next_deltas;
    uint8_t volatile elcon_available;
} BSP;

// The interrupt request priorities, from high to low.
enum {  // STM32G0xx MCUs have 4 interrupt priority levels.
    IRQ_PRIO_PULSE, IRQ_PRIO_SEQ_CLOCK = IRQ_PRIO_PULSE,
    IRQ_PRIO_ADC_DMA, IRQ_PRIO_SYSTICK = IRQ_PRIO_ADC_DMA,
    IRQ_PRIO_USART, IRQ_PRIO_APP_TIMER = IRQ_PRIO_USART,
    IRQ_PRIO_ADC1, IRQ_PRIO_EXTI = IRQ_PRIO_ADC1
};

// Ensure the following three consts refer to the same timer.
static TIM_TypeDef *const pulse_timer = TIM1;   // Advanced 16-bit timer.
static IRQn_Type const pulse_timer_upd_irq = TIM1_BRK_UP_TRG_COM_IRQn;
static IRQn_Type const pulse_timer_cc_irq  = TIM1_CC_IRQn;

// Ensure the following two consts refer to the same timer.
static TIM_TypeDef *const seq_clock  = TIM2;    // General purpose 32-bit timer.
static IRQn_Type const seq_clock_irq = TIM2_IRQn;

// Ensure the following two consts refer to the same timer.
static TIM_TypeDef *const app_timer  = TIM17;   // General purpose 16-bit timer.
static IRQn_Type const app_timer_irq = TIM17_IRQn;

static BSP bsp = {0};

// Using a couple of functions from STM's infamous HAL.
extern HAL_StatusTypeDef HAL_InitTick(uint32_t);
extern void HAL_IncTick(void);


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
        .AHBCLKDivider = LL_RCC_SYSCLK_DIV_1,
        .APB1CLKDivider = RCC_HCLK_DIV1
    };
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, LL_FLASH_LATENCY_2);

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


static void initGPIO()
{
    LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Mode = LL_GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = LL_GPIO_PULL_DOWN;
    GPIO_InitStruct.Pin  = PUSHBUTTON_PIN;
    LL_GPIO_Init(PUSHBUTTON_PORT, &GPIO_InitStruct);
    if (LL_GPIO_IsInputPinSet(PUSHBUTTON_PORT, GPIO_InitStruct.Pin)) {
        uint32_t const *const SYS_MEM = (uint32_t *)0x1FFF0000;
        __set_MSP(SYS_MEM[0]);                  // Set up the bootloader's stackpointer.
        void (*startBootLoader)(void) = (void (*)(void))SYS_MEM[1];
        startBootLoader();                      // This call does not return.
    }

    GPIO_InitStruct.Mode = LL_GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
    GPIO_InitStruct.Pin = TRF_CURR_SENSE_PIN | CAP_VOLT_SENSE_PIN | BAT_VOLT_SENSE_PIN;
    LL_GPIO_Init(ADC_GPIO_PORT, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = VCAP_BUCK_CTRL_PIN;
    LL_GPIO_Init(DAC_GPIO_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
    GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Pin = BUCK_ENABLE_PIN;
    LL_GPIO_ResetOutputPin(BUCK_GPIO_PORT, GPIO_InitStruct.Pin);
    LL_GPIO_Init(BUCK_GPIO_PORT, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = LED_1_PIN;
    LL_GPIO_SetOutputPin(LED_GPIO_PORT, GPIO_InitStruct.Pin);
    LL_GPIO_Init(LED_GPIO_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_OPENDRAIN;
    GPIO_InitStruct.Pin = ALL_TRIAC_PINS;
    LL_GPIO_SetOutputPin(TRIAC_GPIO_PORT, GPIO_InitStruct.Pin);
    LL_GPIO_Init(TRIAC_GPIO_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
    GPIO_InitStruct.Alternate = LL_GPIO_AF_2;   // TIM1_CH1 and TIM1_CH2.
    GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Pin = MOSFET_1_PIN | MOSFET_2_PIN;
    LL_GPIO_ResetOutputPin(MOSFET_GPIO_PORT, GPIO_InitStruct.Pin);
    LL_GPIO_Init(MOSFET_GPIO_PORT, &GPIO_InitStruct);
}


static void initEXTI()
{
    LL_EXTI_InitTypeDef EXTI_InitStruct = {
        .Line_0_31 = LL_EXTI_LINE_4,
        .LineCommand = ENABLE,
        .Mode = LL_EXTI_MODE_IT,
        .Trigger = LL_EXTI_TRIGGER_RISING_FALLING
    };
    LL_EXTI_Init(&EXTI_InitStruct);
    enableInterruptWithPrio(EXTI4_15_IRQn, IRQ_PRIO_EXTI);
}


static void initAppTimer()
{
    app_timer->PSC = SystemCoreClock / APP_TIMER_FREQ_Hz - 1;
    app_timer->CCR1 = BSP_millisecondsToTicks(100);
    app_timer->DIER |= TIM_DIER_CC1IE;          // Interrupt on match with compare register.
    app_timer->EGR |= TIM_EGR_UG;               // Force update of the shadow registers.
    app_timer->CR1 = TIM_CR1_CEN;               // Enable the counter.
    enableInterruptWithPrio(app_timer_irq, IRQ_PRIO_APP_TIMER);
}


static void initSequencerClock()
{
    seq_clock->PSC = SystemCoreClock / SEQUENCER_CLOCK_FREQ_Hz - 1;
    seq_clock->CCMR1 |= TIM_CCMR1_OC1M_0;
    seq_clock->DIER |= TIM_DIER_CC1IE;          // Interrupt on match with compare register.
    seq_clock->EGR |= TIM_EGR_UG;               // Force update of the shadow registers.
    enableInterruptWithPrio(seq_clock_irq, IRQ_PRIO_SEQ_CLOCK);
}


static void initPulseTimer()
{
    pulse_timer->PSC = SystemCoreClock / PULSE_TIMER_FREQ_Hz - 1;
    // PWM mode 1 for channels 1 and 2, enable preload.
    pulse_timer->CCMR1 = TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1PE
                       | TIM_CCMR1_OC2M_2 | TIM_CCMR1_OC2M_1 | TIM_CCMR1_OC2PE;
    // PWM mode 2 for channel 4, which is used to trigger the ADC.
    pulse_timer->CCMR2 = TIM_CCMR2_OC4M_2 | TIM_CCMR2_OC4M_1 | TIM_CCMR2_OC4M_0 | TIM_CCMR2_OC4PE;
    // Enable the outputs.
    pulse_timer->CCER = TIM_CCER_CC1E | TIM_CCER_CC2E | TIM_CCER_CC4E;
    pulse_timer->CR2  = TIM_CR2_CCPC;           // Compare control signals are preloaded.
    pulse_timer->CR1  = TIM_CR1_ARPE | TIM_CR1_URS;
    pulse_timer->DIER = TIM_DIER_UIE;           // Enable update interrupt.
    pulse_timer->BDTR = TIM_BDTR_OSSR | TIM_BDTR_MOE;
    enableInterruptWithPrio(pulse_timer_upd_irq, IRQ_PRIO_PULSE);
    enableInterruptWithPrio(pulse_timer_cc_irq,  IRQ_PRIO_PULSE);
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
    // TODO Wait for the DAC to settle?
    LL_DAC_Enable(DAC1, LL_DAC_CHANNEL_2);
    // LL_DAC_EnableTrigger(DAC1, LL_DAC_CHANNEL_2);
}


static void initADC1()
{
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
    LL_ADC_SetSamplingTimeCommonChannels(ADC1, LL_ADC_SAMPLINGTIME_COMMON_2, LL_ADC_SAMPLINGTIME_39CYCLES_5);

    // Configure a regular channel for the transformer's primary current sensing.
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_1, LL_ADC_CHANNEL_0);
    LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_0, LL_ADC_SAMPLINGTIME_COMMON_1);
    // Configure a regular channel for the capacitor voltage VCAP.
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_2, LL_ADC_CHANNEL_1);
    LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_1, LL_ADC_SAMPLINGTIME_COMMON_2);
    // Configure a regular channel for the battery/supply voltage.
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_3, LL_ADC_CHANNEL_6);
    LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_6, LL_ADC_SAMPLINGTIME_COMMON_2);

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
        .TriggerSource    = LL_ADC_REG_TRIG_EXT_TIM1_CH4,
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


static void initDMAforADC1(uint16_t volatile samples[], size_t nr_of_samples)
{
    LL_DMA_SetPeriphRequest(DMA1, LL_DMA_CHANNEL_1, LL_DMAMUX_REQ_ADC1);
    LL_DMA_ConfigTransfer(DMA1, LL_DMA_CHANNEL_1, LL_DMA_DIRECTION_PERIPH_TO_MEMORY | LL_DMA_MODE_CIRCULAR
                              | LL_DMA_PERIPH_NOINCREMENT | LL_DMA_MEMORY_INCREMENT
                              | LL_DMA_PDATAALIGN_HALFWORD | LL_DMA_MDATAALIGN_HALFWORD
                              | LL_DMA_PRIORITY_HIGH);
    LL_DMA_ConfigAddresses(DMA1, LL_DMA_CHANNEL_1, LL_ADC_DMA_GetRegAddr(ADC1, LL_ADC_DMA_REG_REGULAR_DATA),
                            (uint32_t)samples, LL_DMA_DIRECTION_PERIPH_TO_MEMORY);
    LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_1, nr_of_samples);
    DMA1->IFCR = DMA_IFCR_CGIF1;                // Clear all channel 1 interrupt flags.
    LL_DMA_EnableIT_TC(DMA1, LL_DMA_CHANNEL_1); // Enable transfer complete interrupt.
    LL_DMA_EnableIT_TE(DMA1, LL_DMA_CHANNEL_1); // Enable transfer error interrupt.
    LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_1);
    enableInterruptWithPrio(DMA1_Channel1_IRQn, IRQ_PRIO_ADC_DMA);
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

    // The triac enable signals are active low.
    TRIAC_GPIO_PORT->BSRR
        = (pattern & 1 ? TRIAC_1_PIN << 16 : TRIAC_1_PIN)
        | (pattern & 2 ? TRIAC_2_PIN << 16 : TRIAC_2_PIN)
        | (pattern & 4 ? TRIAC_3_PIN << 16 : TRIAC_3_PIN)
        | (pattern & 8 ? TRIAC_4_PIN << 16 : TRIAC_4_PIN);
}


static void disableOutputStage()
{
    pulse_timer->CCER &= ~(TIM_CCER_CC1E | TIM_CCER_CC2E);
    LL_GPIO_ResetOutputPin(BUCK_GPIO_PORT, BUCK_ENABLE_PIN);
    LL_GPIO_SetOutputPin(TRIAC_GPIO_PORT, ALL_TRIAC_PINS);
}


static uint64_t ticksSinceBoot()
{
    static uint16_t ticks_since_boot[] = { 0, 0, 0, 0 };

    BSP_criticalSectionEnter();                 // Protect our static var.
    uint16_t last_ticks = ticks_since_boot[0];  // Assuming little-Endian.
    if ((ticks_since_boot[0] = app_timer->CNT) < last_ticks) {
        // The 16-bit timer wrapped; increment the second 16-bit word.
        if (++ticks_since_boot[1] == 0) {       // We have a 32-bit wrap too.
            *(uint32_t *)&ticks_since_boot[2] += 1;
        }
    }
    BSP_criticalSectionExit();
    return *(uint64_t *)ticks_since_boot;
}

/**
 * @brief   Calculate the value (0..4095) for the 12-bit DAC to set the desired primary voltage [mV].
 * @note    Assuming R15 = 115 kΩ, R18 = 13 kΩ and R19 = 42.2 kΩ (Refer to the schematic).
 */
#define VPRIM_MIN_mV     1202   //  1202 for Tokmas buck chip, 1064 for SGM 61410.
#define VPRIM_MAX_mV    10195   // 10195 for Tokmas, 10057 for SGM.

static uint16_t Vcap_mV_ToDacVal(uint16_t Vcap_mV)
{
    if (Vcap_mV < VPRIM_MIN_mV) Vcap_mV = VPRIM_MIN_mV;
    else if (Vcap_mV > VPRIM_MAX_mV) Vcap_mV = VPRIM_MAX_mV;
    return (uint16_t)(((1865 * (uint32_t)(VPRIM_MAX_mV - Vcap_mV)) + 2048) / 4096);
}


static void setPrimaryVoltage_mV(uint16_t mV)
{
    bsp.V_prim_mV = mV;
    // TODO Ensure a rising step on the buck regulator's feedback pin does not exceed 80 mV.
    DAC1->DHR12R2 = Vcap_mV_ToDacVal(mV);
}


static void setConfigAndClock(BSP *me)
{
    if (me->pulse_seqnr == pulse_timer->RCR + 1) {
        BSP_setElectrodeConfiguration((uint8_t const *)me->next_burst.elcon);
    } else {
        me->elcon_available = true;
    }
    seq_clock->CCR1 = me->next_burst.start_time_µs;
}


static bool kickOffBurst(BSP *me)
{
    Burst *burst = (Burst *)&me->next_burst;
    if (Burst_isValid(burst)) {
        if (burst->amplitude != 0) {            // 0 means do not change.
            // Scale amplitude 0..255 to 0..10200 mV (for now).
            setPrimaryVoltage_mV(burst->amplitude * 40);
        }
        if ((pulse_timer->CR1 & TIM_CR1_CEN) == 0) {
            return BSP_startBurst(Burst_adjust(burst, 20));
        }
        BSP_logf("Pulse timer busy at t=%u µs\n", seq_clock->CNT);
    }
    EventQueue_postEvent(bsp.delegate, ET_BAD_BURST, (uint8_t const *)burst, sizeof(Burst));
    return false;
}


static void onePulseDone(BSP *me)
{
    if (me->pulse_seqnr++ == pulse_timer->RCR) {
        if (me->elcon_available) {
            BSP_setElectrodeConfiguration((uint8_t const *)me->next_burst.elcon);
            me->elcon_available = false;
        } else {
            LL_GPIO_ResetOutputPin(TRIAC_GPIO_PORT, ALL_TRIAC_PINS);
        }
        EventQueue_postEvent(bsp.delegate, ET_BURST_COMPLETED, NULL, 0);
    }
    // Burst_applyDeltas(&me->next_burst, &me->deltas);
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
        pulse_timer->DIER &= ~(TIM_DIER_CC1IE | TIM_DIER_CC2IE | TIM_DIER_CC4IE);
        pulse_timer->SR &= ~(TIM_SR_UIF | TIM_SR_CC1IF | TIM_SR_CC2IF);
        LL_GPIO_ResetOutputPin(LED_GPIO_PORT, LED_1_PIN);
        // BSP_logf("%s at %u µs\n", __func__, seq_clock->CNT);
        EventQueue_postEvent(bsp.delegate, ET_BURST_EXPIRED, NULL, 0);
    } else {
        BSP_logf("PT SR=0x%x\n", __func__, pulse_timer->SR);
        // spuriousIRQ(&bsp);
    }
}


void TIM1_CC_IRQHandler(void)
{
    if ((pulse_timer->DIER & TIM_DIER_CC1IE) && (pulse_timer->SR & TIM_SR_CC1IF)) {
        pulse_timer->SR &= ~TIM_SR_CC1IF;
        onePulseDone(&bsp);
    }
    if ((pulse_timer->DIER & TIM_DIER_CC2IE) && (pulse_timer->SR & TIM_SR_CC2IF)) {
        pulse_timer->SR &= ~TIM_SR_CC2IF;
        onePulseDone(&bsp);
    }
    pulse_timer->SR &= ~(TIM_SR_CC6IF | TIM_SR_CC5IF | TIM_SR_CC4IF | TIM_SR_CC3IF);
    if (pulse_timer->SR & ~(TIM_SR_CC2IF | TIM_SR_CC1IF | TIM_SR_UIF)) {
        BSP_logf("PT SR=0x%x\n", pulse_timer->SR);
        spuriousIRQ(&bsp);
    }
}


void TIM2_IRQHandler(void)
{
    if (seq_clock->SR & TIM_SR_CC1IF) {
        seq_clock->SR &= ~TIM_SR_CC1IF;         // Clear the interrupt.
        // BSP_logf("TIM2 at %u µs\n", seq_clock->CNT);
        kickOffBurst(&bsp);
    } else {
        spuriousIRQ(&bsp);
    }
}


void TIM17_IRQHandler(void)
{
    if (app_timer->SR & TIM_SR_CC1IF) {         // Capture/compare 1.
        app_timer->SR &= ~TIM_SR_CC1IF;         // Clear the interrupt.
        bsp.app_timer_handler(bsp.app_timer_target, BSP_microsecondsSinceBoot());
        app_timer->CCR1 += bsp.clock_ticks_per_app_timer_tick;
    } else {
        spuriousIRQ(&bsp);
    }
}


void EXTI4_15_IRQHandler(void)
{
    if (EXTI->RPR1 & PUSHBUTTON_PIN) {          // Rising?
        EXTI->RPR1 = PUSHBUTTON_PIN;
        invokeSelector(&bsp.button_sel, 1);
    } else if (EXTI->FPR1 & PUSHBUTTON_PIN) {   // Falling?
        EXTI->FPR1 = PUSHBUTTON_PIN;
        invokeSelector(&bsp.button_sel, 0);
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
        // BSP_logf("DMA1_1\n");
        // EventQueue_postEvent(bsp.delegate, ET_ADC_DATA_AVAILABLE, (uint8_t const *)bsp.adc_1_samples, sizeof bsp.adc_1_samples);
    } else if (DMA1->ISR & DMA_ISR_TEIF1) {
        DMA1->IFCR = DMA_IFCR_CTEIF1;           // Clear transfer error flag.
        BSP_logf("%s, TEIF1\n", __func__);
        LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_1);
    } else {
        BSP_logf("%s, ISR=0x%x\n", __func__, DMA1->ISR);
        DMA1->IFCR = DMA_IFCR_CHTIF1;
    }
}

/*
 * Support for STM32Cube LL drivers.
 */

void assert_failed(char const *filename, uint32_t line_nr)
{
    BSP_assertionFailed(filename, line_nr, "STM LL");
}

/*
 * Below are the functions implementing this module's interface.
 */

void BSP_init()
{
    RCC->IOPENR = RCC_IOPENR_GPIOAEN | RCC_IOPENR_GPIOBEN | RCC_IOPENR_GPIOCEN;
    initGPIO();

    LL_RCC_SetADCClockSource(LL_RCC_ADC_CLKSOURCE_HSI);
    RCC->APBENR1 = RCC_APBENR1_TIM2EN | RCC_APBENR1_TIM3EN | RCC_APBENR1_PWREN | RCC_APBENR1_DAC1EN;
    RCC->APBENR2 = RCC_APBENR2_TIM1EN | RCC_APBENR2_TIM16EN | RCC_APBENR2_TIM17EN | RCC_APBENR2_ADCEN | RCC_APBENR2_SYSCFGEN;
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;
    // Enable instruction cache and prefetch buffer.
    FLASH->ACR |= FLASH_ACR_ICEN | FLASH_ACR_PRFTEN;

    HAL_InitTick(IRQ_PRIO_SYSTICK);
    SystemClock_Config();
    BSP_logf("DevId=0x%x, SystemCoreClock=%u\n", LL_DBGMCU_GetDeviceID(), SystemCoreClock);
    initDAC();
    initDMAforADC1(bsp.adc_1_samples, M_DIM(bsp.adc_1_samples));
    initADC1();
    LL_ADC_Enable(ADC1);
    LL_ADC_REG_StartConversion(ADC1);
}


char const *BSP_firmwareVersion()
{
    return "v0.61-beta";
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


void BSP_registerAppTimerHandler(void (*handler)(void *, uint64_t), void *target, uint32_t microseconds_per_app_timer_tick)
{
    bsp.app_timer_handler = handler;
    bsp.app_timer_target  = target;
    bsp.clock_ticks_per_app_timer_tick = microseconds_per_app_timer_tick * TICKS_PER_MICROSECOND;
    initAppTimer();
}


void BSP_registerPulseDelegate(EventQueue *dq)
{
    bsp.delegate = dq;
    initPulseTimer();
    initSequencerClock();
}


void BSP_registerButtonHandler(Selector *sel)
{
    bsp.button_sel = *sel;
    initEXTI();
}


void BSP_enableUartInterrupt(int intr)
{
    enableInterruptWithPrio((IRQn_Type)intr, IRQ_PRIO_USART);
}


void BSP_toggleTheLED()
{
    LL_GPIO_TogglePin(LED_GPIO_PORT, LED_1_PIN);
}


void BSP_primaryVoltageEnable(bool must_be_on)
{
    if ((BUCK_GPIO_PORT->ODR & BUCK_ENABLE_PIN) == 0) {
        if (must_be_on) {
            BSP_logf("Turning Vcap ON\n");
            LL_GPIO_SetOutputPin(BUCK_GPIO_PORT, BUCK_ENABLE_PIN);
        }
    } else {
        if (! must_be_on) {
            BSP_logf("Turning Vcap OFF\n");
            LL_GPIO_ResetOutputPin(BUCK_GPIO_PORT, BUCK_ENABLE_PIN);
        }
    }
}


void BSP_setElectrodeConfiguration(uint8_t const elcon[2])
{
    setSwitches(elcon[0] | elcon[1]);
}


uint16_t BSP_setPrimaryVoltagePercent(uint8_t perc)
{
    setPrimaryVoltage_mV(perc * 85);
    return bsp.V_prim_mV;
}


void BSP_startSequencerClock(uint32_t time_µs)
{
    BSP_logf("%s(%d)\n", __func__, time_µs);
    seq_clock->CCR1 = time_µs - 1;
    seq_clock->CNT = time_µs;
    seq_clock->CR1 |= TIM_CR1_CEN;              // Enable the counter.
}


void BSP_stopSequencerClock()
{
    BSP_logf("%s at %u µs\n", __func__, seq_clock->CNT);
    seq_clock->CR1 &= ~TIM_CR1_CEN;             // Disable the counter.
    setSwitches(ALL_TRIACS_OFF);
}


void BSP_resumeSequencerClock()
{
    BSP_logf("%s at %u µs\n", __func__, seq_clock->CNT);
    BSP_setElectrodeConfiguration((uint8_t const *)bsp.next_burst.elcon);
    seq_clock->CR1 |= TIM_CR1_CEN;              // Enable the counter.
}


bool BSP_scheduleBurst(Burst const *burst)
{
    // Accept the burst only if there is enough time ('do less sooner').
    // Minimum margin yet to be determined; trying 20 µs.
    BSP_criticalSectionEnter();
    if ((int32_t)seq_clock->CNT < (int32_t)burst->start_time_µs - 20) {
        bsp.next_burst = *burst;                // Copy.
        setConfigAndClock(&bsp);
        BSP_criticalSectionExit();
        return true;
    }
    BSP_criticalSectionExit();

    BSP_logf("%s clock=%d, t=%d\n", __func__, seq_clock->CNT, burst->start_time_µs);
    EventQueue_postEvent(bsp.delegate, ET_BAD_BURST, (uint8_t const *)burst, sizeof(Burst));
    return false;
}


bool BSP_startBurst(Burst const *burst)
{
    pulse_timer->ARR = burst->pace_µs - 1;
    pulse_timer->RCR = burst->nr_of_pulses - 1;
    bsp.pulse_seqnr = 0;
    pulse_timer->CNT = 0;
    pulse_timer->SR &= ~(TIM_SR_CC1IF | TIM_SR_CC2IF);
    uint8_t phase = Burst_phase(burst);
    uint8_t pulse_width_µs = Burst_pulseWidth_µs(burst);
    if (phase == 0) {
        pulse_timer->CCR1 = pulse_width_µs;
        pulse_timer->DIER |= TIM_DIER_CC1IE;
    } else if (phase == 1) {
        pulse_timer->CCR2 = pulse_width_µs;
        pulse_timer->DIER |= TIM_DIER_CC2IE;
    } else return false;                        // We only have one output stage.

    if (pulse_width_µs > 10) pulse_timer->CCR4 = 10;
    EventQueue_postEvent(bsp.delegate, ET_BURST_STARTED, (uint8_t const *)&seq_clock->CNT, sizeof seq_clock->CNT);
    pulse_timer->EGR |= TIM_EGR_UG;             // Force update of the shadow registers.
    // Keep at least one instruction between the shadow register update and clearing the CCRs.
    bsp.elcon_available = false;
    pulse_timer->CCR1 = 0;
    pulse_timer->CCR2 = 0;
    pulse_timer->CCR4 = 0;
    pulse_timer->CR1 |= TIM_CR1_CEN;            // Enable the counter.
    return true;
}


void BSP_triggerADC(void)
{
    BSP_logf("%s\n", __func__);
    uint16_t const *v = (uint16_t const *)bsp.adc_1_samples;
    AdcValues const av = {
        .Vbat_mV = ((uint32_t)v[2] * 52813UL) / 16384,
        .Vcap_mV = ((uint32_t)v[1] * 52813UL) / 16384,
        .Iprim_mA = ((uint32_t)v[0] * 2063UL) /  1024,
    };
    EventQueue_postEvent(bsp.delegate, ET_ADC_DATA_AVAILABLE, (uint8_t const *)&av, sizeof av);
}


void BSP_close()
{
    __disable_irq();
}


void BSP_sleepMCU()
{
    HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
}


void BSP_assertionFailed(char const *filename, unsigned int line_number, char const *predicate)
{
    disableOutputStage();
    BSP_logf("ASSERTION FAILED (%s : %u) %s\n", filename, line_number, predicate);
    // TODO Register the problem and then reboot.
    while (1) { /* Stay here. */ }
}


void BSP_shutDown()
{
    disableOutputStage();
    // HAL_Delay(1000);
    LL_GPIO_ResetOutputPin(LED_GPIO_PORT, LED_1_PIN);
    // HAL_PWREx_EnterSHUTDOWNMode();
    while (1) {}                                // Not reached.
}
