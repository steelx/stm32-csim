#include <stdint.h>

/* Symbols supplied by our linker script, not ordinary C variables. */
extern uint32_t _stack_top, _sidata, _sdata, _edata, _sbss, _ebss;
int main(void);
void Reset_Handler(void);
static void Default_Handler(void) { for (;;) {} }

/* An ARM vector table begins with an initial stack address, followed by
 * function addresses. Cortex-M function pointers have the Thumb bit set.
 * uintptr_t is 32 bits for THIS ARM compilation, regardless of host width.
 * We provide core vectors for a useful image; interrupts are not simulated. */
__attribute__((section(".isr_vector"), used))
const uintptr_t vectors[16] = {
    (uintptr_t)&_stack_top, (uintptr_t)Reset_Handler,
    (uintptr_t)Default_Handler, (uintptr_t)Default_Handler,
    (uintptr_t)Default_Handler, (uintptr_t)Default_Handler,
    (uintptr_t)Default_Handler, 0, 0, 0, 0,
    (uintptr_t)Default_Handler, (uintptr_t)Default_Handler, 0,
    (uintptr_t)Default_Handler, (uintptr_t)Default_Handler
};

void Reset_Handler(void)
{
    /* Initialized globals live in SRAM, with their initial bytes in Flash. */
    const uint32_t *source = &_sidata;
    for (uint32_t *p = &_sdata; p < &_edata; ++p) *p = *source++;
    /* Uninitialized/static globals must start at zero. */
    for (uint32_t *p = &_sbss; p < &_ebss; ++p) *p = 0;
    (void)main();
    for (;;) {}                   /* Bare-metal main has nowhere to return. */
}
