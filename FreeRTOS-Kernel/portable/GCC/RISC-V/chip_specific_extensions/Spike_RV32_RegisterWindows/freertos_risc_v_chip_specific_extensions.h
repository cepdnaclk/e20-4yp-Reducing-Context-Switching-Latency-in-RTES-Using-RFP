/*
 * FreeRTOS Kernel - FYP Spike RV32 Register Windows
 * Chip-specific extensions for use with Spike ISA simulator (--isa=rv32gc)
 * and the expanded Physical Register File (64 GPRs) with CSR 0x800/0x801
 * for zero-overhead context switch via partition/window switch only.
 *
 * Include this in the assembler include path when building for Spike+FYP.
 */

#ifndef __FREERTOS_RISC_V_EXTENSIONS_H__
#define __FREERTOS_RISC_V_EXTENSIONS_H__

#define portasmUSE_REGISTER_WINDOWS       1
#define portasmHAS_SIFIVE_CLINT           1
#define portasmHAS_MTIME                  1
#define portasmADDITIONAL_CONTEXT_SIZE    0

/* CSR 0x800 = read-only current window config (base|size). */
/* CSR 0x801 = staged config for next context; on mret HW applies it. */
#define portasmCSR_WINDOW_ACTIVE  0x800
#define portasmCSR_WINDOW_STAGED  0x801

.macro portasmSAVE_ADDITIONAL_REGISTERS
/* No additional registers beyond GPRs; window state is in CSRs. */
   .endm

   .macro portasmRESTORE_ADDITIONAL_REGISTERS
   .endm

#endif /* __FREERTOS_RISC_V_EXTENSIONS_H__ */
