.section .text.init
.global _start    # <--- This line is critical for the linker to find the entry
.global tohost

_start:
  # 1. Setup Stack Pointer (at 0x80010000)
  li sp, 0x80010000
  
  # 2. Jump to C code
  call main

  # 3. Stop Simulation (write 1 to tohost)
  li t0, 1
  la t1, tohost
  sw t0, 0(t1)
1: j 1b

.section .tohost
.align 6
tohost: .dword 0