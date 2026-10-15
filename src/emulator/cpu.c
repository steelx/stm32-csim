#include "cpu.h"
#include "stm32/f446/memory_map.h"
#include <stdio.h>
#include <string.h>

static void mmio_fault(cpu_t *cpu, uint32_t address, unsigned size)
{
    cpu->fault = true;
    snprintf(cpu->error, sizeof(cpu->error), "Unsupported MMIO: 0x%08X (%u bytes)", address, size);
    (void)uc_emu_stop(cpu->engine);
}
static uint64_t mmio_read(uc_engine *uc, uint64_t offset, unsigned size, void *user)
{
    (void)uc;
    cpu_t *cpu = user;
    uint32_t address = F446_GPIOA_BASE + (uint32_t)offset, value = 0;
    if (!device_access(&cpu->device, address, size, false, &value)) mmio_fault(cpu, address, size);
    return value;
}
static void mmio_write(uc_engine *uc, uint64_t offset, unsigned size, uint64_t value, void *user)
{
    (void)uc;
    cpu_t *cpu = user;
    uint32_t address = F446_GPIOA_BASE + (uint32_t)offset, word = (uint32_t)value;
    if (!device_access(&cpu->device, address, size, true, &word)) mmio_fault(cpu, address, size);
}
static bool checked(cpu_t *cpu, uc_err result)
{
    if (result == UC_ERR_OK) return true;
    cpu->fault = true;
    snprintf(cpu->error, sizeof(cpu->error), "%s", uc_strerror(result));
    return false;
}
void cpu_close(cpu_t *cpu)
{
    if (cpu->engine) uc_close(cpu->engine);
    cpu->engine = NULL;
}
bool cpu_open(cpu_t *cpu, const firmware_t *firmware)
{
    memset(cpu, 0, sizeof(*cpu));
    device_reset(&cpu->device);
    /* Choose the core BEFORE mapping memory or initializing the engine.
     * Unicorn executes instructions; our callbacks supply every peripheral. */
    if (!checked(cpu, uc_open(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS, &cpu->engine))) return false;
    if (!checked(cpu, uc_ctl_set_cpu_model(cpu->engine, UC_CPU_ARM_CORTEX_M4)) ||
        !checked(cpu, uc_mem_map(cpu->engine, F446_FLASH_BASE, F446_FLASH_SIZE, UC_PROT_READ | UC_PROT_EXEC)) ||
        !checked(cpu, uc_mem_map(cpu->engine, F446_SRAM_BASE, F446_SRAM_SIZE, UC_PROT_READ | UC_PROT_WRITE)) ||
        !checked(cpu, uc_mem_write(cpu->engine, F446_FLASH_BASE, firmware->bytes, firmware->size)) ||
        !checked(cpu, uc_mmio_map(cpu->engine, F446_GPIOA_BASE, 0x4000U, mmio_read, cpu, mmio_write, cpu))) {
        cpu_close(cpu); return false;
    }
    uint32_t sp = firmware->stack, pc = firmware->reset, xpsr = 0x01000000U;
    if (!checked(cpu, uc_reg_write(cpu->engine, UC_ARM_REG_MSP, &sp)) ||
        !checked(cpu, uc_reg_write(cpu->engine, UC_ARM_REG_XPSR, &xpsr)) ||
        !checked(cpu, uc_reg_write(cpu->engine, UC_ARM_REG_PC, &pc))) {
        cpu_close(cpu); return false;
    }
    return true;
}
uint32_t cpu_pc(cpu_t *cpu)
{
    uint32_t pc = 0;
    if (cpu->engine) (void)uc_reg_read(cpu->engine, UC_ARM_REG_PC, &pc);
    return pc;
}
bool cpu_slice(cpu_t *cpu, size_t instructions)
{
    if (!cpu->engine || cpu->fault || instructions == 0U) return false;
    /* Thumb bit is required on EVERY resume. Count bounds even while(1) {}.
     * A 20 ms timeout additionally bounds a slice on a slow host. */
    uc_err result = uc_emu_start(cpu->engine, (uint64_t)cpu_pc(cpu) | 1U, UINT64_MAX, 20000, instructions);
    ++cpu->slices;
    if (!cpu->fault) (void)checked(cpu, result);
    return !cpu->fault;
}
