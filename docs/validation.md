# Validation record

Validated locally on Windows x86-64, 2026-10-15.

- Host compiler: MSVC 19.38.33145.0 (Visual Studio 2022).
- Firmware compiler: Clang 19.1.5, `--target=arm-none-eabi -mcpu=cortex-m4 -mthumb`,
  LLD, soft-float, freestanding, no libc/device library.
- Application code: C17, `/W4 /WX`; ARM sources: `-Wall -Wextra -Werror`.
- Unicorn: 2.1.4, built from its pinned source, only ARM guest support enabled.
- GUI: unchanged upstream WebView with the installed WebView2 runtime.

## Final build and automated checks

Command: `./scripts/build.ps1 -GuiTests`

Result: configure and build succeeded without application compiler warnings.

```text
1/2 core_and_arm_execution .... Passed (0.80 sec)
2/2 native_webview_smoke ...... Passed (2.41 sec)
100% tests passed, 0 tests failed out of 2
```

The first CTest entry contains checks for bit extraction, GPIOA clock gating,
all MODER modes, ODR transitions, BSRR set/reset priority and readback, trace ring
wrap, digital nets/conflicts, disconnected LED ground, ARM `.data`/`.bss` startup,
CPU reset, bounded stepping, unsupported MMIO failure, and real firmware blinking.
It loads compiled ARM binaries, not hand-interpreted C or inferred source behavior.

The second entry opens a real native WebView window. Its JavaScript test requests
Run through the C binding, observes LED HIGH/ON and LOW/OFF in both returned C state
and SVG class/text, pauses, verifies stable slice count, steps one instruction,
then resets and checks zero accesses/slices and Hi-Z PA5. It closes automatically.

## Manual native-window inspection

The normal executable was launched on the interactive Windows desktop. Run was
clicked through native UI automation. Screenshots showed both:

- PA5 LOW, LED dark, GPIOA_ODR `0x00000000`.
- PA5 HIGH, LED glowing green, GPIOA_ODR `0x00000020`.

RCC_AHB1ENR displayed `0x00000001`, GPIOA_MODER displayed `0xA8000400`, and the
register panel showed the actual ODR read/write operations and bit explanations.
The layout, source textarea and register panel were visually inspected in the
1280×900 client area. The window closed normally before the final rebuild.

## Repository audit and untested scope

The original sibling WebView checkout, the WebView submodule and the Unicorn
submodule all report clean source trees. No dependency implementation was edited.
Generated builds, fetched SDK files, WebView profiles and temporary PDFs stay
under ignored build/tmp directories. Application source changes are left available
for review; no application commit or remote was created.

macOS/Apple Silicon execution has not been tested. This is a GPIO/LED proof of
concept, not full STM32 emulation or an electrical/cycle-accurate simulator.
The complete list of deliberate limitations is in [hardware.md](hardware.md).
