#include <stdint.h>
/* Exercise .data and .bss startup as well as the real ARM MMIO path. */
volatile uint32_t initialized = 0x12345678U;
volatile uint32_t zero_initialized;
int main(void)
{
    if (initialized != 0x12345678U || zero_initialized != 0U) for (;;) {}
    *(volatile uint32_t *)0x40023830U = 1U;
    *(volatile uint32_t *)0x40020000U = 1U << 10U;
    *(volatile uint32_t *)0x40020014U = 1U << 5U;
    for (;;) {}
}
