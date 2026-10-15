# STM32-Csim 0.1

A small educational STM32F446RE bare-metal circuit simulator. Real Clang-compiled
ARM/Thumb firmware runs in Unicorn's Cortex-M4 core. C models turn its RCC/GPIO
accesses into PA5 and circuit state. A native WebView shows a green LED blinking
and explains the register accesses that caused it.

This milestone is the complete **PA5 → 330 Ω resistor → LED → GND** experiment.
It includes Run, Pause, Reset, one-instruction Step, a source viewer and a register
activity panel. It does not include a component designer or other peripherals.

## Build and run on Windows

Prerequisites: Git, CMake 3.24+, Visual Studio 2022 C++ build tools/Windows SDK,
LLVM tools (`clang`, `ld.lld`, `llvm-objcopy`), and Microsoft Edge WebView2 Runtime.
Visual Studio's optional C++ Clang tools satisfy the LLVM requirement. The host
build uses MSVC; firmware is always compiled separately by Clang for ARM.
On first configure, unchanged upstream WebView CMake fetches its WebView2 SDK.

From the repository root in PowerShell:

```powershell
git submodule update --init --recursive
./scripts/build.ps1 -GuiTests
./build/windows/Debug/stm32-csim.exe
```

The script locates Visual Studio's LLVM installation when Clang is not on PATH.
It builds, runs core/ARM tests and, with `-GuiTests`, opens a native smoke-test
window that closes automatically. Omit `-GuiTests` without an interactive desktop.
Close the normal application before rebuilding its executable on Windows.

Manual equivalent, with LLVM on PATH:

```powershell
cmake --preset windows -DCSIM_GUI_TESTS=ON
cmake --build --preset windows --parallel
ctest --preset windows
```

The application starts **paused**, before Reset_Handler executes. Press **Run**.
Pause takes effect at the next slice boundary. Step executes one ARM instruction,
not one C statement. Reset pauses and restores the original loaded image and all
modeled state. Blink timing depends on host scheduling, not STM32 clock frequency.

## Edit real firmware

Edit `firmware/main.c`, close the application, rebuild, and relaunch. The build
produces `firmware.elf` (useful for disassembly/debug symbols) and `firmware.bin`
(loaded by the emulator). The build always refreshes the binary and read-only
source view next to the executable, even when no host C file changed.

To use another source file:

```powershell
cmake --preset windows -DFIRMWARE_SOURCE=path/to/user/main.c
cmake --build --preset windows --parallel
```

Restore the default with `-DFIRMWARE_SOURCE=firmware/main.c` (an absolute path is
also accepted). Core tests compile their own copy of the default blink example,
so choosing a different application source does not alter the regression fixture.
The GUI smoke test expects a blinking image; use the default for that test.
There is no compiler or shell exposed to JS.

An already-built compatible image can also be loaded with:

```powershell
./build/windows/Debug/stm32-csim.exe --firmware path/to/image.bin
```

The source panel then states that the external image has no bundled source.
The raw image must be linked at 0x08000000, start with valid initial MSP/Thumb reset
vectors, fit the 512 KiB Flash, and use only the supported memory/peripheral subset.
Addresses in that firmware are **guest STM32 addresses**, never Windows pointers.

## macOS Apple Silicon

The C core is host-independent. The platform layer uses pthreads and WebView uses
Cocoa/WebKit. This path is provided for the planned Mac development, **not yet
validated on a Mac**. Install Xcode Command Line Tools and Homebrew LLVM/CMake/Ninja:

```sh
brew install llvm cmake ninja pkg-config
export PATH="$(brew --prefix llvm)/bin:$PATH"
git submodule update --init --recursive
cmake --preset macos
cmake --build --preset macos
ctest --preset macos
./build/macos/stm32-csim
```

The preset selects the ARM64 **host**; the guest remains Cortex-M4. Homebrew LLVM
supplies LLD and llvm-objcopy rather than relying on Apple's linker for bare metal.
The preset's ARCHFLAGS also tells upstream Unicorn to build the native ARM64 host
instead of its default universal macOS library.

## What is modeled

| Area | v0.1 behavior |
| --- | --- |
| CPU | Unicorn Cortex-M4, Thumb/Thumb-2, soft-float firmware |
| Memory | 512 KiB Flash, 128 KiB contiguous SRAM; explicit vector loading |
| RCC | AHB1ENR.GPIOAEN only |
| GPIOA | MODER, low 16 ODR bits, BSRR; PA5 connected to the circuit |
| Circuit | C components/pins/wires/nets, Hi-Z/LOW/HIGH/conflict, digital resistor, LED |
| Activity | Both reads and writes, raw values, before/after values and PA5 explanations |
| Scheduling | Worker requests 100,000-instruction slices; 20 ms timeout; 8 ms pacing |
| UI | Native WebView, HTML/CSS/vanilla JS/SVG; read-only source textarea |

Only aligned 32-bit accesses to the four modeled registers are accepted. Unsupported
MMIO or memory operations stop with an error. Input/AF/analog modes and a disabled
GPIO clock give virtual Hi-Z. Clock gating, resistor behavior and timing are
deliberate educational simplifications. There is no voltage/current solver.
See [the full hardware evidence and limitations](docs/hardware.md).

There is no HAL, LL, CMSIS device header, Arduino, CubeMX, Qt, Electron, Rust,
frontend framework or JS CPU/peripheral simulator. Application-owned native code
is C17. Only the unchanged WebView dependency requires C++.

## Repository preservation and dependency pins

Initial inspection found that `STM32-Csim` contained only an empty `third_party`
directory and was **not a Git repository**. The sibling `webview` directory was a
clean upstream `webview/webview` clone. It remains untouched, including its history.
A new application Git repository was initialized here. The existing local clone
was used to populate `third_party/webview` as a submodule; its recorded URL was then
set to upstream so other machines can initialize it. No library source was modified.

| Dependency | Exact pin | License |
| --- | --- | --- |
| [WebView](https://github.com/webview/webview) | `cbbdee44afff22867de9fd88a9fc8350d9bdd399` | MIT |
| [Unicorn 2.1.4](https://github.com/unicorn-engine/unicorn/releases/tag/2.1.4) | `8028ec436f2d9376525352dd38ed9ed6b9f6be10` | GPLv2; see its COPYING/COPYING.LGPL2 and individual files |

Unicorn's checked-in `include/unicorn/unicorn.h`, `include/unicorn/arm.h` and
`docs/COMPILE.md` were inspected before integration. The implementation uses
`uc_open(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)`,
`uc_ctl_set_cpu_model(..., UC_CPU_ARM_CORTEX_M4)`, `uc_mmio_map`, and bounded
`uc_emu_start`. Unicorn does **not** supply STM32 peripheral simulation.

**Licensing implications:** this build statically links GPLv2 Unicorn. Distributing
the combined application requires a GPL-compatible distribution, applicable license
notices and corresponding source/build materials under the GPL's terms. Merely
switching to dynamic linking should not be treated as a way to avoid these duties.
Private local use does not itself require publishing source. WebView's MIT notices
must also be preserved. No separate license for application-owned files has been
selected in this milestone; select a compatible license before redistribution.
Read [Unicorn's license](third_party/unicorn/COPYING) and the
[GNU GPLv2 FAQ](https://www.gnu.org/licenses/old-licenses/gpl-2.0-faq.html).

## Verification and where to study

`tests/test_core.c` verifies bit extraction, RCC enable/gating, all MODER encodings,
ODR and BSRR transitions, net conflict handling, disconnected-ground behavior, LED
state, ARM startup `.data`/`.bss`, reset, stepping and the real blinking firmware.
Checks stay enabled in Release builds. The native `--smoke-test` additionally
checks the rendered SVG LED and Run/Pause/Step/Reset through WebView's C bindings.

Start with [firmware/main.c](firmware/main.c), then [gpio.c](src/stm32/f446/gpio.c),
[device.c](src/stm32/f446/device.c), [circuit.c](src/circuit/circuit.c), and
[cpu.c](src/emulator/cpu.c). The [learning notes](docs/learning-notes.md) explain
the C concepts; [architecture](docs/architecture.md) shows ownership and data flow.
The [validation record](docs/validation.md) records what was actually run.
