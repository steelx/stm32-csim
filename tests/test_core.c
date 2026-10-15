#include "emulator/cpu.h"
#include "stm32/f446/memory_map.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); return 1; } } while (0)
static bool write_register(device_t *d, uint32_t address, uint32_t value)
{ return device_access(d, address, 4, true, &value); }
int main(int argc, char **argv)
{
    CHECK(argc == 4);
    CHECK(register_field(0xC00U, 10, 3) == 3U);
    CHECK(register_field(0x20U, 5, 1) == 1U);
    CHECK(register_field(0xFFFFFFFFU, 32, 1) == 0U);
    device_t d;
    device_reset(&d);
    CHECK(d.gpioa.moder == 0xA8000000U && !rcc_gpioa_enabled(&d.rcc));
    CHECK(write_register(&d, F446_GPIOA_ODR, 32U));
    CHECK(d.gpioa.odr == 0U && d.pa5 == SIM_HIZ && !d.circuit.led_on);
    CHECK(write_register(&d, F446_RCC_AHB1ENR, 1));
    CHECK(rcc_gpioa_enabled(&d.rcc));
    for (unsigned mode = 0; mode < 4; ++mode) {
        CHECK(write_register(&d, F446_GPIOA_MODER, mode << 10U));
        CHECK(gpio_mode(&d.gpioa, 5) == mode);
        CHECK(d.pa5 == (mode == 1U ? SIM_LOW : SIM_HIZ));
    }
    CHECK(write_register(&d, F446_GPIOA_MODER, 1U << 10U));
    CHECK(write_register(&d, F446_GPIOA_ODR, 32U));
    CHECK(d.pa5 == SIM_HIGH && d.circuit.led_on);
    CHECK(write_register(&d, F446_GPIOA_BSRR, 32U << 16U));
    CHECK(d.pa5 == SIM_LOW && !d.circuit.led_on);
    CHECK(write_register(&d, F446_GPIOA_BSRR, (32U << 16U) | 32U));
    CHECK(d.pa5 == SIM_HIGH); /* simultaneous set wins */
    uint32_t bsrr = 99;
    CHECK(device_access(&d, F446_GPIOA_BSRR, 4, false, &bsrr) && bsrr == 0);
    for (unsigned i = 0; i < 80; ++i) CHECK(write_register(&d, F446_GPIOA_ODR, i));
    CHECK(d.trace[(d.accesses - 1U) % TRACE_CAPACITY].sequence == d.accesses);
    CHECK(write_register(&d, F446_RCC_AHB1ENR, 0));
    CHECK(d.pa5 == SIM_HIZ && !d.circuit.led_on);
    uint32_t value = 123;
    CHECK(device_access(&d, F446_GPIOA_ODR, 4, false, &value) && value == 0);
    CHECK(!device_access(&d, F446_GPIOA_ODR, 1, false, &value));
    CHECK(!device_access(&d, F446_GPIOA_BASE + 4U, 4, false, &value));
    CHECK(net_resolve(SIM_HIZ, SIM_HIGH) == SIM_HIGH);
    CHECK(net_resolve(SIM_LOW, SIM_HIGH) == SIM_CONFLICT);
    CHECK(led_is_on(SIM_HIGH, SIM_LOW));
    CHECK(!led_is_on(SIM_HIGH, SIM_HIZ) && !led_is_on(SIM_CONFLICT, SIM_LOW));
    circuit_t c;
    circuit_init(&c);
    circuit_update(&c, SIM_HIGH);
    CHECK(c.led_on);
    c.wire_count = 2; /* disconnect ground: no return path */
    circuit_update(&c, SIM_HIGH);
    CHECK(!c.led_on);

    firmware_t fw;
    char error[256] = {0};
    CHECK(firmware_load(&fw, argv[1], error, sizeof(error)));
    cpu_t cpu;
    CHECK(cpu_open(&cpu, &fw));
    CHECK(cpu_slice(&cpu, 2000));
    CHECK(cpu.device.pa5 == SIM_HIGH && cpu.device.circuit.led_on);
    CHECK(cpu.device.accesses == 3U);
    CHECK(cpu.device.trace[0].address == F446_RCC_AHB1ENR);
    CHECK(cpu.device.trace[1].address == F446_GPIOA_MODER);
    CHECK(cpu.device.trace[2].address == F446_GPIOA_ODR);
    uint32_t pc = cpu_pc(&cpu);
    CHECK(pc >= F446_FLASH_BASE && pc < F446_FLASH_BASE + F446_FLASH_SIZE);
    CHECK(cpu_slice(&cpu, 1));
    cpu_close(&cpu);
    CHECK(cpu_open(&cpu, &fw));
    CHECK(cpu.device.accesses == 0U && cpu.device.pa5 == SIM_HIZ);
    cpu_close(&cpu); firmware_free(&fw);
    CHECK(firmware_load(&fw, argv[3], error, sizeof(error)));
    CHECK(cpu_open(&cpu, &fw));
    CHECK(!cpu_slice(&cpu, 2000));
    CHECK(cpu.fault && strstr(cpu.error, "0x40020004") != NULL);
    CHECK(!cpu_slice(&cpu, 1));
    cpu_close(&cpu); firmware_free(&fw);
    CHECK(!firmware_load(&fw, "this-firmware-file-does-not-exist.bin", error, sizeof(error)));

    CHECK(firmware_load(&fw, argv[2], error, sizeof(error)));
    CHECK(cpu_open(&cpu, &fw));
    bool high = false, low_after_high = false;
    for (unsigned i = 0; i < 600 && !low_after_high; ++i) {
        CHECK(cpu_slice(&cpu, 100000));
        if (cpu.device.circuit.led_on) high = true;
        if (high && cpu.device.pa5 == SIM_LOW) low_after_high = true;
    }
    CHECK(high && low_after_high);
    printf("PASS: ARM main.c executed, PA5 HIGH -> LOW, LED ON -> OFF (%llu slices)\n", (unsigned long long)cpu.slices);
    cpu_close(&cpu); firmware_free(&fw);
    puts("PASS: fields, RCC, MODER, ODR, BSRR, clock gating, nets, LED, startup, reset, step, trace wrap, MMIO fault");
    return 0;
}
