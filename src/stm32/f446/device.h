#ifndef CSIM_DEVICE_H
#define CSIM_DEVICE_H
#include "rcc.h"
#include "gpio.h"
#define TRACE_CAPACITY 64U
typedef struct {
    uint64_t sequence;
    bool write, accepted;
    uint32_t address, value, before, after;
    const char *name;
    char meaning[144];
} register_event_t;
typedef struct {
    rcc_t rcc;
    gpio_t gpioa;
    circuit_t circuit;
    register_event_t trace[TRACE_CAPACITY];
    uint64_t accesses, transitions;
    sim_signal_t pa5;
} device_t;
void device_reset(device_t *device);
bool device_access(device_t *device, uint32_t address, unsigned size, bool write, uint32_t *value);
#endif
