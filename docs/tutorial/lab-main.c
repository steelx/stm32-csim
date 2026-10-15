/* Module 1 companion: NUCLEO-F446RE, real hardware only.
 * Use an STM32F446RE C project with ST startup, linker script and CMSIS.
 * Replace its main.c with this file. No HAL initialization is needed.
 * LAB_MODE 0: blink PA5; 1: PA10 button controls PA5; 2: PA5 held high.
 * Delay is intentionally uncalibrated. Measure it with the logic analyzer.
 */
#include "stm32f446xx.h"

#ifndef LAB_MODE
#define LAB_MODE 0
#endif

#if LAB_MODE < 0 || LAB_MODE > 2
#error "LAB_MODE must be 0, 1 or 2"
#endif

static void delay(void)
{
    for (volatile unsigned int i = 0; i < 500000U; ++i) {
        __NOP();
    }
}

int main(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    (void)RCC->AHB1ENR;

    /* Load the low output value before enabling the output driver. */
    GPIOA->BSRR = 1U << (5U + 16U);
    GPIOA->OTYPER &= ~(1U << 5U);          /* Push-pull. */
    GPIOA->OSPEEDR &= ~(3U << (5U * 2U)); /* Low slew-rate setting. */
    GPIOA->PUPDR &= ~(3U << (5U * 2U));   /* No pull resistor. */
    GPIOA->MODER = (GPIOA->MODER & ~(3U << (5U * 2U)))
                  | (1U << (5U * 2U));   /* PA5 output. */

    GPIOA->MODER &= ~(3U << (10U * 2U)); /* PA10 input. */
    GPIOA->PUPDR &= ~(3U << (10U * 2U)); /* External 10k pull-up. */

    for (;;) {
#if LAB_MODE == 0
        GPIOA->BSRR = 1U << 5U;
        delay();
        GPIOA->BSRR = 1U << (5U + 16U);
        delay();
#elif LAB_MODE == 1
        /* Active-low button: pressed -> output high. No debounce yet. */
        if ((GPIOA->IDR & (1U << 10U)) == 0U) {
            GPIOA->BSRR = 1U << 5U;
        } else {
            GPIOA->BSRR = 1U << (5U + 16U);
        }
#else
        GPIOA->BSRR = 1U << 5U;
#endif
    }
}
