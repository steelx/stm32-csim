#include "rcc.h"
void rcc_reset(rcc_t *rcc) { rcc->ahb1enr = 0U; }
bool rcc_gpioa_enabled(const rcc_t *rcc) { return (rcc->ahb1enr & 1U) != 0U; }
void rcc_write(rcc_t *rcc, uint32_t value)
{
    /* Only GPIOAEN is implemented. Other clocks have no model in v0.1. */
    rcc->ahb1enr = value & 1U;
}
