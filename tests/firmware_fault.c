#include <stdint.h>
int main(void)
{
    /* OTYPER is deliberately outside the v0.1 model. It must fault clearly. */
    *(volatile uint32_t *)0x40020004U = 0U;
    for (;;) {}
}
