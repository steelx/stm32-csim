# Hardware evidence and v0.1 boundaries

The authority is STMicroelectronics **RM0390**, STM32F446xx reference manual:
[official ST manual](https://www.st.com/resource/en/reference_manual/rm0390-stm32f446xx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf).
ST's [documentation listing](https://www.st.com/en/microcontrollers-microprocessors/stm32f446/documentation.html)
advertised revision 9 when this milestone was built. Its download endpoints timed out
in this environment. The actual pages checked were **RM0390 revision 6, March 2021**,
retrieved from an [unchanged ST PDF mirrored by WeActStudio](https://github.com/WeActStudio/WeActStudio.STM32F4_64Pin_CoreBoard/blob/master/Doc/RM0390%20STM32F446xx%20Reference%20manual.pdf).
Revision 9 has not been compared; page numbers below deliberately identify revision 6.
The supplied STM32F446XC.PDF is DS10693 / DocID027107 revision 6 (2016), a device
datasheet, not the peripheral reference manual. It confirms the xE density and MCU family.

| Item | Implemented value | RM0390 rev 6 evidence |
| --- | --- | --- |
| Flash | 0x08000000, 512 KiB for RE | §3.3, Table 4, p. 65; device density from datasheet |
| SRAM | 0x20000000–0x2001FFFF, 128 KiB | Figure 2 p. 57, §2.2.3 p. 60 |
| SRAM banks | 112 KiB SRAM1 + 16 KiB SRAM2 | Figure 2 p. 57; modeled as contiguous RAM |
| RCC base | 0x40023800 | Table 1 p. 58 |
| GPIOA base | 0x40020000 | Table 1 p. 58 |
| RCC_AHB1ENR | offset 0x30; reset 0; GPIOAEN bit 0 | §6.3.10 pp. 144–145 |
| GPIOA_MODER | offset 0; reset 0xA8000000 | §7.4.1 p. 187 |
| MODER5 | bits 11:10; 00 input, 01 output, 10 AF, 11 analog | §7.4.1 p. 187 |
| GPIOA_ODR | offset 0x14; reset 0; low 16 bits | §7.4.6 p. 190 |
| GPIOA_BSRR | offset 0x18; low 16 set, high 16 reset; set wins | §7.4.7 p. 190 |
| Push-pull default | OTYPER reset is zero | §7.4.2 p. 188 |

## Explicit simplifications

- Only RCC_AHB1ENR bit 0 is stored. Other RCC enable bits read zero; no PLL,
  clock-frequency calculation, reset-controller logic or clock-start latency.
- Only GPIOA MODER, ODR and BSRR are modeled. PA5 is the only connected MCU pin.
  MODER/ODR keep their implemented port-wide fields; explanations focus on PA5.
- Accesses must be aligned **32-bit words**. Real hardware also accepts byte and
  halfword accesses for these registers. Unsupported sizes/registers stop the emulator.
- With GPIOA clock disabled, writes are ignored and reads return zero. Disabling
  the clock also forces the virtual pin to Hi-Z. That last rule is an educational
  gating rule, not a claim that real hardware releases previously configured outputs.
- General-purpose outputs assume reset-default push-pull behavior. OTYPER,
  OSPEEDR, PUPDR, IDR, alternate functions and input sampling are not implemented.
  Input/AF/analog modes produce Hi-Z in this model. Debug pins are not connected.
- SRAM is one zero-filled Unicorn allocation. There is no bus arbitration, RAM
  execution, backup SRAM or bit-band alias. Flash is read/execute only; no programming.
- The loader explicitly reads the initial MSP and Thumb reset vector at Flash base.
  It does not emulate BOOT pins, the address-zero Flash alias or hardware reset timing.
  Reset recreates Unicorn and SRAM, then reruns startup from the original binary.
- Interrupt delivery, NVIC, SysTick and exception-driven firmware are unsupported.
  Unicorn may implement core instructions internally, but this project supplies no
  STM32 interrupt-controller or exception service model. Firmware uses soft-float.
- Instruction counts bound execution; they are not cycles. A 20 ms host timeout is
  an additional bound. Each running slice requests 100,000 instructions, then the
  worker sleeps 8 ms for visible pacing. Delay-loop time is not hardware time.
- Wires merge pin groups. A positive resistor bridges two groups for digital
  propagation and retains its resistance as metadata. It does not calculate current,
  voltage, brightness, resistor power or damage. Zero resistance is flagged invalid
  in this educational model. The LED is on only for HIGH anode and LOW cathode.
- Trace storage is a bounded ring of 64 accesses; older events are overwritten.
  Snapshots arrive roughly every 60 ms, so very short pulses need not be visible.
- The board drawing is Nucleo-inspired, not a complete Nucleo board simulation.
  The external R1/D1 circuit is hard-coded. No designer or project persistence.

These boundaries are intentional. Add a peripheral only with its reference-manual
evidence, a C model, tests, and an explicit update to this table.
