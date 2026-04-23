/*
 * FreeRTOS RISC-V chip-specific extensions for CVA6 with FYP register windows.
 * Kernel executes in window 0; tasks may use dedicated windows via CSR 0x801.
 */

#ifndef __FREERTOS_RISC_V_EXTENSIONS_H__
#define __FREERTOS_RISC_V_EXTENSIONS_H__

#define portasmUSE_REGISTER_WINDOWS       1
#define portasmHAS_SIFIVE_CLINT           0
#define portasmHAS_MTIME                  0
#define portasmADDITIONAL_CONTEXT_SIZE    0
#define portasmENABLE_MINIMAL_SAVE        1

#define portasmCSR_WINDOW_ACTIVE          0x800
#define portasmCSR_WINDOW_STAGED          0x801

.macro portasmSAVE_ADDITIONAL_REGISTERS
.endm

.macro portasmRESTORE_ADDITIONAL_REGISTERS
.endm

#endif /* __FREERTOS_RISC_V_EXTENSIONS_H__ */
