#ifndef CSIM_GPIO_H
#define CSIM_GPIO_H
#include "circuit/circuit.h"
#include <stdint.h>
typedef struct { uint32_t moder, odr; } gpio_t;
void gpio_reset(gpio_t *gpio);
unsigned gpio_mode(const gpio_t *gpio, unsigned pin);
sim_signal_t gpio_pin(const gpio_t *gpio, bool clock_enabled, unsigned pin);
void gpio_bsrr(gpio_t *gpio, uint32_t value);
#endif
