#ifndef CSIM_CPU_H
#define CSIM_CPU_H
#include "firmware.h"
#include "stm32/f446/device.h"
#include <unicorn/unicorn.h>
typedef struct {
    uc_engine *engine;
    device_t device;
    bool fault;
    char error[256];
    uint64_t slices;
} cpu_t;
bool cpu_open(cpu_t *cpu, const firmware_t *firmware);
void cpu_close(cpu_t *cpu);
bool cpu_slice(cpu_t *cpu, size_t instructions);
uint32_t cpu_pc(cpu_t *cpu);
#endif
