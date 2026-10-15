#include "gpio.h"
#include "memory_map.h"
void gpio_reset(gpio_t *gpio)
{
    /* GPIOA's debug pins are alternate function after reset, not all input. */
    gpio->moder = UINT32_C(0xA8000000);
    gpio->odr = 0U;
}
unsigned gpio_mode(const gpio_t *gpio, unsigned pin)
{
    return pin < 16U ? register_field(gpio->moder, pin * 2U, 3U) : 0U;
}
sim_signal_t gpio_pin(const gpio_t *gpio, bool clock_enabled, unsigned pin)
{
    if (pin >= 16U || !clock_enabled || gpio_mode(gpio, pin) != 1U) return SIM_HIZ;
    return (gpio->odr & (1U << pin)) != 0U ? SIM_HIGH : SIM_LOW;
}
void gpio_bsrr(gpio_t *gpio, uint32_t value)
{
    /* RM0390: if BSx and BRx are both set, BSx has priority. */
    gpio->odr = ((gpio->odr & ~(value >> 16U)) | (value & 0xFFFFU)) & 0xFFFFU;
}
