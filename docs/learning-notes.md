# Learning C with the GPIO-to-LED experiment

Read these files in order. Follow one ODR write all the way to the LED before
adding more hardware.

1. `firmware/main.c`: what the STM32 programmer writes.
2. `src/stm32/f446/memory_map.h`, `rcc.c`, `gpio.c`: bit fields and pin meaning.
3. `src/stm32/f446/device.c`: register access becomes a state change and an explanation.
4. `src/circuit/circuit.c`: a digital pin becomes a connected component's input.
5. `src/emulator/cpu.c`: why compiled ARM code can call our C peripheral model.
6. `firmware/startup.c` and `stm32f446re.ld`: what happens before `main`.
7. `src/app/webview_bridge.c`: ownership, threads and UI snapshots.

## A number is not necessarily a host pointer

In the firmware, `0x40023830U` is a 32-bit STM32 bus address. Casting it to
`volatile uint32_t *` tells the ARM compiler to emit a 32-bit memory access.
Dereferencing reads or writes that register. It is safe here because the result
executes in the guest. The host stores this same address as a **number** and
dispatches it through a `switch`, never as a pointer to Windows memory.

`volatile` asks the compiler to preserve observable accesses. It does not make
an operation atomic and it is not a thread synchronization mechanism. The app
uses mutexes for synchronization instead.

## Shifts, masks, and read-modify-write

PA5's two mode bits begin at `5 * 2 = 10`. `3U << 10U` produces a mask for bits
11:10. AND with its complement clears those bits; OR with `1U << 10U` writes 01.
Other pins retain their values. An `ODR ^= 1U << 5U` statement emits a register
read, an XOR in the CPU, then a register write. You can see both accesses in the UI.

`register_field(value, shift, mask)` shifts a field down then removes unrelated
bits. It guards against shifts of 32 or more because those have undefined behavior
for a 32-bit value. Unsigned fixed-width types make bit operations predictable.

BSRR is a hardware command register: writing one bit changes ODR without the
firmware doing a read-modify-write. It reads as zero. The model gives set bits
priority if both set and reset are requested, as RM0390 specifies.

## Structs model state; enums name possibilities

`gpio_t` holds two register values. `device_t` owns RCC, GPIO, circuit and trace
state. A pointer parameter such as `gpio_t *gpio` lets a function modify its caller's
object without copying it. `const gpio_t *gpio` promises not to modify through
that pointer. `sim_signal_t` names Hi-Z, LOW, HIGH and conflict; it avoids treating
an undriven pin as the number zero.

Circuit pins identify components and resolved nets by small array indexes. Wires
merge groups. The resistor merges two groups only as a digital abstraction. Multiple
conflicting drivers resolve to `SIM_CONFLICT`; the LED does not light on conflict
or Hi-Z. This is intentionally much smaller than an analog circuit solver.

## Callbacks connect libraries to our program

Unicorn calls `mmio_write` when an ARM instruction writes into our MMIO mapping.
The callback signature is defined by Unicorn's C API. Its `void *user` carries
the `cpu_t` pointer supplied when registering the mapping. Converting that pointer
back is valid because we know the original object type and keep it alive until
Unicorn closes. No global singleton is necessary.

## Startup, sections, and byte order

The linker puts vectors and code in Flash. Initialized globals have their initial
bytes in Flash and runtime storage in SRAM (`.data`). Startup copies those bytes
and zeros `.bss`. The first vector supplies MSP; the second supplies Reset_Handler
with its Thumb bit. The emulator decodes these little-endian bytes explicitly so
the host's byte order cannot silently change their meaning.

The startup code uses compiler section attributes because ISO C has no way to
place an array at the vector-table address. Those small, documented extensions
are separate from the ordinary C17 simulator code.

## Ownership and bounded work

`firmware_load` allocates bytes and `firmware_free` releases them. `cpu_open` creates
a Unicorn handle and `cpu_close` releases it, including after a partial failure.
The worker owns the engine. The UI reads copies. This clear ownership prevents
the UI and emulation thread from mutating the same register object concurrently.

A super-loop never returns, so `cpu_slice` asks Unicorn to execute only a bounded
number of instructions. This is cooperative scheduling by the host, not an STM32
timer interrupt. Mutexes protect only commands/snapshots, never an entire emulation
slice. The UI remains able to process Pause even while firmware spins forever.

## Arrays, ring buffers, and strings

Register events live in a fixed array. `sequence % TRACE_CAPACITY` chooses the next
slot; once full, the oldest event is overwritten. The UI receives at most 64 events,
which keeps memory and rendering bounded. JSON strings are escaped and formatted
with explicit capacities; firmware source is inserted into a textarea as text,
never interpreted as JavaScript.

## Tests are small experiments

`tests/test_core.c` uses checks that remain active even in Release builds. It tests
the register rules directly, then loads actual ARM binaries. The probe's globals
verify `.data`/`.bss` startup before enabling PA5. The normal blink example must
produce both HIGH and LOW and corresponding LED states. The native WebView smoke
test exercises C bindings and observes the rendered SVG class/text before checking
Pause, Step and Reset.
