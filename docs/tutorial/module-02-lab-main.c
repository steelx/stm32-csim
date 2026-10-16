/* Module 2 signal fixture: NUCLEO-F446RE / STM32F446RE only.
 * Keep ST startup, linker script and CMSIS system file. Replace main.c.
 * Polling only: no interrupt handlers, HAL, heap or floating point.
 * 0 PWM PA8; 1 complementary PA8/PA7; 2 slow Hall PB4/5/6;
 * 3 fast Hall; 4 quadrature PA8/PA7; 5 fixed servo pulse;
 * 6 OPTIONAL TB6612FNG motor A, held-to-run button PA10 -> GND.
 * Disconnect actuators when selecting any signal-only mode.
 * Nominal HSI clock, not a calibrated frequency reference.
 */
#include "stm32f446xx.h"
#ifndef LAB_MODE
#define LAB_MODE 0
#endif
#ifndef PWM_PERCENT
#define PWM_PERCENT 25
#endif
#ifndef SERVO_US
#define SERVO_US 1500
#endif
#ifndef ENCODER_EDGE_US
#define ENCODER_EDGE_US 1250
#endif
#if LAB_MODE < 0 || LAB_MODE > 6
#error "LAB_MODE must be 0..6"
#endif
#if PWM_PERCENT < 0 || PWM_PERCENT > 100
#error "PWM_PERCENT must be 0..100"
#endif
#if SERVO_US < 1400 || SERVO_US > 1600
#error "This introductory fixture limits SERVO_US to 1400..1600"
#endif
#if ENCODER_EDGE_US < 100 || ENCODER_EDGE_US > 1000000
#error "ENCODER_EDGE_US must be 100..1000000"
#endif

static void clock_16mhz(void)
{
    RCC->CR |= RCC_CR_HSION;
    while ((RCC->CR & RCC_CR_HSIRDY) == 0U) { }
    RCC->CFGR &= ~RCC_CFGR_SW;
    while ((RCC->CFGR & RCC_CFGR_SWS) != 0U) { }
    /* Now running HSI; set AHB/APB1/APB2 to divide by 1. */
    RCC->CFGR &= ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2);
    RCC->DCKCFGR &= ~RCC_DCKCFGR_TIMPRE;
    SystemCoreClock = 16000000U;
    SysTick->CTRL = 0U;
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    (void)RCC->APB1ENR;
    RCC->APB1RSTR |= RCC_APB1RSTR_TIM2RST;
    RCC->APB1RSTR &= ~RCC_APB1RSTR_TIM2RST;
    TIM2->PSC = 15U; TIM2->ARR = 0xFFFFFFFFU;
    TIM2->EGR = TIM_EGR_UG; TIM2->SR = 0U;
    TIM2->CR1 = TIM_CR1_CEN;
}

static void wait_us(uint32_t us)
{
    const uint32_t start = TIM2->CNT;
    while ((uint32_t)(TIM2->CNT - start) < us) { }
}

static void output(GPIO_TypeDef *port, uint32_t pin)
{
    port->BSRR = 1U << (pin + 16U);
    port->OTYPER &= ~(1U << pin);
    port->OSPEEDR &= ~(3U << (pin * 2U));
    port->PUPDR &= ~(3U << (pin * 2U));
    port->MODER = (port->MODER & ~(3U << (pin * 2U))) | (1U << (pin * 2U));
}

#if LAB_MODE == 0 || LAB_MODE == 1 || LAB_MODE == 5 || LAB_MODE == 6
static void af1(uint32_t pin)
{
    output(GPIOA, pin);
    const uint32_t idx = pin / 8U, shift = (pin % 8U) * 4U;
    GPIOA->AFR[idx] = (GPIOA->AFR[idx] & ~(15U << shift)) | (1U << shift);
    GPIOA->MODER = (GPIOA->MODER & ~(3U << (pin * 2U))) | (2U << (pin * 2U));
}

static void pwm(uint32_t period_us, uint32_t high_us, uint32_t complementary)
{
    RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;
    (void)RCC->APB2ENR;
    RCC->APB2RSTR |= RCC_APB2RSTR_TIM1RST;
    RCC->APB2RSTR &= ~RCC_APB2RSTR_TIM1RST;
    TIM1->PSC = 15U; TIM1->ARR = period_us - 1U; TIM1->CCR1 = high_us;
    TIM1->CCMR1 = (6U << 4U) | TIM_CCMR1_OC1PE; /* PWM mode 1 */
    TIM1->CR1 = TIM_CR1_ARPE; /* edge aligned, CKD=00 */
    TIM1->CCER = TIM_CCER_CC1E | (complementary ? TIM_CCER_CC1NE : 0U);
    TIM1->BDTR = complementary ? 32U : 0U; /* 32 / 16 MHz = 2 us */
    TIM1->EGR = TIM_EGR_UG; TIM1->SR = 0U;
    af1(8U);
    if (complementary) { af1(7U); }
    TIM1->BDTR |= TIM_BDTR_MOE;
    TIM1->CR1 |= TIM_CR1_CEN;
}
#endif

int main(void)
{
    clock_16mhz();
#if LAB_MODE == 0
    pwm(1000U, 10U * PWM_PERCENT, 0U);
#elif LAB_MODE == 1
    pwm(1000U, 500U, 1U);
#elif LAB_MODE == 2 || LAB_MODE == 3
    const uint8_t hall[6] = {1U, 5U, 4U, 6U, 2U, 3U};
    output(GPIOB, 4U); output(GPIOB, 5U); output(GPIOB, 6U);
    uint32_t state = 0U;
    for (;;) {
        const uint32_t bits = ((hall[state] & 4U) << 2U) |
                              ((hall[state] & 2U) << 4U) |
                              ((hall[state] & 1U) << 6U);
        GPIOB->BSRR = bits | ((0x70U & ~bits) << 16U);
        state = (state + 1U) % 6U;
        wait_us(LAB_MODE == 2 ? 1000000U : 4167U);
    }
#elif LAB_MODE == 4
    const uint8_t q[4] = {0U, 2U, 3U, 1U}; /* A=PA8, B=PA7 */
    output(GPIOA, 8U); output(GPIOA, 7U);
    uint32_t state = 0U;
    for (;;) {
        for (uint32_t direction = 0U; direction < 2U; ++direction) {
            for (uint32_t edge = 0U; edge < 160U; ++edge) {
                wait_us(ENCODER_EDGE_US);
                state = (state + (direction == 0U ? 1U : 3U)) % 4U;
                const uint32_t bits = ((q[state] & 2U) << 7U) | ((q[state] & 1U) << 7U);
                GPIOA->BSRR = bits | ((0x180U & ~bits) << 16U);
            }
            wait_us(2000000U); /* stopped shaft example */
        }
    }
#elif LAB_MODE == 5
    pwm(20000U, SERVO_US, 0U); /* signal-only until servo verified */
#else
    /* Optional TB6612: AIN1 PB4, AIN2 PB5, STBY PB6, PWMA PA8.
     * External 10k pulldown on STBY is required during reset/debug.
     * PA10 input uses an internal pull-up; button to GND = held to run.
     * Never debug/halt with motor power connected: PWM may keep running.
     */
    output(GPIOB, 4U); output(GPIOB, 5U); output(GPIOB, 6U);
    GPIOB->BSRR = 1U << 4U; /* fixed direction, AIN2 remains low */
    GPIOA->MODER &= ~(3U << 20U);
    GPIOA->PUPDR = (GPIOA->PUPDR & ~(3U << 20U)) | (1U << 20U);
    pwm(1000U, 10U * PWM_PERCENT, 0U);
    /* Require release after reset, then a deliberate press. */
    while ((GPIOA->IDR & (1U << 10U)) == 0U) { }
    for (;;) {
        if ((GPIOA->IDR & (1U << 10U)) == 0U) {
            GPIOB->BSRR = 1U << 6U;
        } else {
            GPIOB->BSRR = 1U << 22U;
        }
        wait_us(1000U);
    }
#endif
    for (;;) { wait_us(1000000U); }
}
