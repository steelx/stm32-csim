#include "device.h"
#include "memory_map.h"
#include <stdio.h>
#include <string.h>

void device_reset(device_t *d)
{
    memset(d, 0, sizeof(*d));
    rcc_reset(&d->rcc);
    gpio_reset(&d->gpioa);
    circuit_init(&d->circuit);
    d->pa5 = SIM_HIZ;
}
bool device_access(device_t *d, uint32_t address, unsigned size, bool write, uint32_t *value)
{
    const char *modes[] = {"Input", "General purpose output", "Alternate function", "Analog"};
    const char *name;
    uint32_t before, after;
    if (size != 4U || (address & 3U) != 0U) return false;
    switch (address) {
    case F446_RCC_AHB1ENR: name = "RCC_AHB1ENR"; before = d->rcc.ahb1enr; break;
    case F446_GPIOA_MODER: name = "GPIOA_MODER"; before = d->gpioa.moder; break;
    case F446_GPIOA_ODR: name = "GPIOA_ODR"; before = d->gpioa.odr; break;
    case F446_GPIOA_BSRR: name = "GPIOA_BSRR"; before = d->gpioa.odr; break;
    default: return false;
    }
    bool enabled = address == F446_RCC_AHB1ENR || rcc_gpioa_enabled(&d->rcc);
    if (write && enabled) {
        switch (address) {
        case F446_RCC_AHB1ENR: rcc_write(&d->rcc, *value); break;
        case F446_GPIOA_MODER: d->gpioa.moder = *value; break;
        case F446_GPIOA_ODR: d->gpioa.odr = *value & 0xFFFFU; break;
        case F446_GPIOA_BSRR: gpio_bsrr(&d->gpioa, *value); break;
        }
    }
    after = address == F446_RCC_AHB1ENR ? d->rcc.ahb1enr :
            address == F446_GPIOA_MODER ? d->gpioa.moder : d->gpioa.odr;
    if (!write) *value = !enabled || address == F446_GPIOA_BSRR ? 0U : after;
    register_event_t *e = &d->trace[d->accesses % TRACE_CAPACITY];
    *e = (register_event_t){++d->accesses, write, enabled, address, *value, before, after, name, {0}};
    if (!enabled) {
        snprintf(e->meaning, sizeof(e->meaning), "GPIOA clock disabled: %s", write ? "write ignored" : "read returns zero");
    } else if (address == F446_RCC_AHB1ENR) {
        snprintf(e->meaning, sizeof(e->meaning), "GPIOAEN: %u -> %u", before & 1U, after & 1U);
    } else if (address == F446_GPIOA_MODER) {
        unsigned old_mode = register_field(before, 10U, 3U), new_mode = register_field(after, 10U, 3U);
        snprintf(e->meaning, sizeof(e->meaning), "MODER5: %u%u -> %u%u | PA5: %s -> %s",
                 old_mode >> 1U, old_mode & 1U, new_mode >> 1U, new_mode & 1U, modes[old_mode], modes[new_mode]);
    } else {
        snprintf(e->meaning, sizeof(e->meaning), "ODR5: %u -> %u", register_field(before, 5U, 1U), register_field(after, 5U, 1U));
    }
    sim_signal_t next = gpio_pin(&d->gpioa, rcc_gpioa_enabled(&d->rcc), 5U);
    if (next != d->pa5) ++d->transitions;
    d->pa5 = next;
    circuit_update(&d->circuit, next);
    return true;
}
