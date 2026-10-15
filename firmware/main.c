#include <stdint.h>

/* RM0390: RCC on AHB1 at 0x40023800, GPIOA at 0x40020000.
 * These pointers belong to the simulated STM32, NEVER to the host PC. */
#define RCC_BASE   0x40023800U
#define GPIOA_BASE 0x40020000U
#define RCC_AHB1ENR (*(volatile uint32_t *)(RCC_BASE + 0x30U))
#define GPIOA_MODER (*(volatile uint32_t *)(GPIOA_BASE + 0x00U))
#define GPIOA_ODR   (*(volatile uint32_t *)(GPIOA_BASE + 0x14U))

int main(void)
{
    RCC_AHB1ENR |= 1U;             /* Bit 0: GPIOAEN. */
    (void)RCC_AHB1ENR;             /* Read back after enabling the clock. */
    GPIOA_MODER &= ~(3U << 10U);   /* PA5 owns bits 11:10. */
    GPIOA_MODER |=  (1U << 10U);   /* 01: general-purpose output. */

    for (;;) {
        GPIOA_ODR ^= 1U << 5U;
        /* Educational delay only: instruction slices are not MCU cycles. */
        for (volatile uint32_t i = 0; i < 500000U; ++i) {
        }
    }
}
