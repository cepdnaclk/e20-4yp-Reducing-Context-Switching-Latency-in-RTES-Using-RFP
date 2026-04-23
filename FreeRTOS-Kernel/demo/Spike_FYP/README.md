# FreeRTOS RISC-V port with FYP Register Windows (Spike)

This demo builds FreeRTOS for the **Spike ISA simulator** with the FYP modifications: expanded Physical Register File (64 GPRs), CSR 0x800 (current window), and CSR 0x801 (staged window for context switch). Context switch for tasks using a register window is **partition switch only** (no GPR save/restore to memory).

## Requirements

- **Toolchain:** `riscv32-unknown-elf-gcc` (e.g. from SiFive or riscv-collab)
- **Simulator:** Spike with FYP patches (register windows, 0x800/0x801, trap/mret behaviour)
- Build and run on your **remote environment** where Spike has been modified as per FYP_Spike_Modifications.txt

## Configuration

- **FreeRTOSConfig.h:** `configUSE_RISCV_REGISTER_WINDOWS = 1`
- **Chip extensions:** Use the path  
  `portable/GCC/RISC-V/chip_specific_extensions/Spike_RV32_RegisterWindows`  
  in the **assembler** include path so that `freertos_risc_v_chip_specific_extensions.h` defines `portasmUSE_REGISTER_WINDOWS` and CSR 0x800/0x801.

## Build

From this directory (`demo/Spike_FYP`):

```bash
make
```

This compiles the kernel (tasks, queue, list), the RISC-V port (port.c, portASM.S) with the Spike chip extensions, plus startup.S and main.c.

If your kernel is not at `../..` relative to this folder, set `KERNEL` in the Makefile.

## Run on Spike

```bash
spike --isa=rv32gc demo.elf
```

Spike must be the **modified** version that implements:

- 64 physical integer registers and window mapping
- On trap: save current window to 0x801 and force window to 0 (kernel)
- On mret: apply 0x801 to the register mapping

## Behaviour

- **Task A** is created without a register window; it uses the **legacy** context switch (full GPR save/restore).
- **Task B** is created and then assigned a register window with `vPortTaskSetRegisterWindow( xB, 32, 32 )` (base 32, size 32). It uses the **zero-overhead** path: on switch, only CSR 0x801 is set and `mret` restores the partition; no memory load/store of GPRs.

So the application can mix:

- Tasks **with** a register window (assign with `vPortTaskSetRegisterWindow`) → partition-only switch.
- Tasks **without** a register window → normal 32-register save/restore (same as standard FreeRTOS RISC-V port).

## API (when `configUSE_RISCV_REGISTER_WINDOWS == 1`)

```c
void vPortTaskSetRegisterWindow( void * xTask, uint32_t ulBase, uint32_t ulSize );
```

Call after creating the task (e.g. after `xTaskCreateStatic`). `ulBase` is the physical window base (e.g. 0 = kernel, 32 = first task window), `ulSize` is the window size (typically 32). Format stored: `(size << 16) | base` for CSR 0x801.

## Testing

1. **Round-robin:** Run the demo; you should see alternating “[TASK A]” and “[TASK B]” messages. Task B uses the windowed path.
2. **Latency / workload:** Use the same pattern as in FYP_Tests.txt (mcycle around ecall, compare baseline vs windowed) in your own test app linked with this port.

## Files changed in the kernel (summary)

- **portable/GCC/RISC-V/portmacro.h**  
  - `configUSE_RISCV_REGISTER_WINDOWS`, `portWINDOW_CFG_OFFSET`, `vPortTaskSetRegisterWindow` declaration.
- **portable/GCC/RISC-V/port.c**  
  - `vPortTaskSetRegisterWindow` implementation.
- **portable/GCC/RISC-V/portContext.h**  
  - When `portasmUSE_REGISTER_WINDOWS` is set: 32-word context, `portWINDOW_CFG_OFFSET`; minimal save (mepc, mstatus, nesting, 0x801) and minimal restore (0x801, mepc, mstatus, nesting, mret) for windowed tasks.
- **portable/GCC/RISC-V/portASM.S**  
  - One extra word in `pxPortInitialiseStack` for `window_cfg` when register windows are enabled.
- **portable/GCC/RISC-V/chip_specific_extensions/Spike_RV32_RegisterWindows/freertos_risc_v_chip_specific_extensions.h**  
  - New chip header: `portasmUSE_REGISTER_WINDOWS`, `portasmCSR_WINDOW_ACTIVE`, `portasmCSR_WINDOW_STAGED`.
