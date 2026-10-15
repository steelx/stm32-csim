#ifndef CSIM_MEMORY_MAP_H
#define CSIM_MEMORY_MAP_H
#include <stdint.h>

/* RM0390 memory map and RCC/GPIO register descriptions. See docs/hardware.md. */
#define F446_FLASH_BASE UINT32_C(0x08000000)
#define F446_FLASH_SIZE UINT32_C(0x00080000)
#define F446_SRAM_BASE  UINT32_C(0x20000000)
#define F446_SRAM_SIZE  UINT32_C(0x00020000)
#define F446_GPIOA_BASE UINT32_C(0x40020000)
#define F446_RCC_BASE   UINT32_C(0x40023800)
#define F446_RCC_AHB1ENR (F446_RCC_BASE + 0x30U)
#define F446_GPIOA_MODER (F446_GPIOA_BASE + 0x00U)
#define F446_GPIOA_ODR   (F446_GPIOA_BASE + 0x14U)
#define F446_GPIOA_BSRR  (F446_GPIOA_BASE + 0x18U)

static inline uint32_t register_field(uint32_t value, unsigned shift, uint32_t mask)
{
    return shift < 32U ? (value >> shift) & mask : 0U;
}
#endif
