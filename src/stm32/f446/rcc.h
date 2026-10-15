#ifndef CSIM_RCC_H
#define CSIM_RCC_H
#include <stdbool.h>
#include <stdint.h>
typedef struct { uint32_t ahb1enr; } rcc_t;
void rcc_reset(rcc_t *rcc);
bool rcc_gpioa_enabled(const rcc_t *rcc);
void rcc_write(rcc_t *rcc, uint32_t value);
#endif
