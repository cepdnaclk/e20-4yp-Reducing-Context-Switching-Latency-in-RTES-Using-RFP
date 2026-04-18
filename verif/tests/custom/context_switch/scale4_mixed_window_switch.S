/* Four-context scaling test with mixed window sizes over 64 physical regs. */

  .section .text.init
  .globl _start
_start:
  la      sp, stack_top
  j       main

  .section .text
  .globl main
main:
  la      t0, trap_entry
  csrw    mtvec, t0
  csrw    0x320, zero

  csrr    t0, 0xB00
  li      t1, 32
scm_cycle_spin:
  addi    t1, t1, -1
  bnez    t1, scm_cycle_spin
  csrr    t2, 0xB00
  beq     t2, t0, mcycle_fail

  # deterministic signature prefill (guaranteed parse points)
  li      t0, 0x120
  sw      zero, 0(t0)
  sw      zero, 4(t0)
  sw      zero, 8(t0)
  sw      zero, 12(t0)
  sw      zero, 16(t0)
  sw      zero, 20(t0)
  sw      zero, 24(t0)
  sw      zero, 28(t0)
  sw      zero, 32(t0)
  sw      zero, 36(t0)
  sw      zero, 40(t0)
  sw      zero, 44(t0)

  # loop countdown
  li      t0, 0x130
  li      t1, 64
  sw      t1, 0(t0)
  # sum / min / max
  li      t0, 0x134
  sw      zero, 0(t0)
  li      t0, 0x138
  li      t1, -1
  sw      t1, 0(t0)
  li      t0, 0x13c
  sw      zero, 0(t0)
  # rotating index 0..3
  li      t0, 0x140
  sw      zero, 0(t0)
  # heartbeat
  li      t0, 0x118
  sw      zero, 0(t0)
  la      t0, acc_trap_entry
  sw      zero, 0(t0)
  la      t0, acc_trap_body
  sw      zero, 0(t0)
  la      t0, acc_mret_return
  sw      zero, 0(t0)

scale_loop:
  li      t0, 0x130
  lw      t1, 0(t0)
  beqz    t1, scale_done

  li      t0, 0x140
  lw      t0, 0(t0)
  beqz    t0, scale_cfg0
  li      t1, 1
  beq     t0, t1, scale_cfg1
  li      t1, 2
  beq     t0, t1, scale_cfg2
  # idx3: size 20, base 36
  li      t1, 0x00140024
  j       scale_stage
scale_cfg0:
  # idx0: size 8, base 0
  li      t1, 0x00080000
  j       scale_stage
scale_cfg1:
  # idx1: size 12, base 8
  li      t1, 0x000c0008
  j       scale_stage
scale_cfg2:
  # idx2: size 16, base 20
  li      t1, 0x00100014

scale_stage:
  li      t0, 0x11c
  sw      t1, 0(t0)
  csrw    0x801, t1
  csrr    t2, 0xB00
  la      t0, trap_start_cycle
  sw      t2, 0(t0)
  csrr    t2, 0xB00
  ecall
  csrr    t0, 0xB00
  sub     t2, t0, t2
  la      t1, trap_entry_cycle
  lw      t3, 0(t1)
  la      t1, trap_start_cycle
  lw      t4, 0(t1)
  sub     t5, t3, t4
  la      t1, acc_trap_entry
  lw      t4, 0(t1)
  add     t4, t4, t5
  sw      t4, 0(t1)
  la      t1, mret_start_cycle
  lw      t3, 0(t1)
  sub     t5, t0, t3
  la      t1, acc_mret_return
  lw      t4, 0(t1)
  add     t4, t4, t5
  sw      t4, 0(t1)

  li      t0, 0x11c
  lw      t0, 0(t0)
  csrr    t1, 0x800
  bne     t1, t0, scm_cfg_skip

  li      t0, 0x134
  lw      t1, 0(t0)
  add     t1, t1, t2
  sw      t1, 0(t0)
scm_cfg_skip:

  li      t0, 0x138
  lw      t1, 0(t0)
  bltu    t2, t1, scm_min_upd
scm_min_done:
  li      t0, 0x13c
  lw      t1, 0(t0)
  bltu    t1, t2, scm_max_upd
scm_max_done:
  li      t0, 0x130
  lw      t2, 0(t0)
  addi    t2, t2, -1
  sw      t2, 0(t0)

  li      t0, 0x140
  lw      t1, 0(t0)
  addi    t1, t1, 1
  li      t2, 4
  blt     t1, t2, scale_idx_store
  li      t1, 0
scale_idx_store:
  sw      t1, 0(t0)
  li      t0, 0x118
  sw      t1, 0(t0)
  j       scale_loop

scale_done:
  li      t2, 64

  # return to full window before print path
  li      t0, 0x00200000
  csrw    0x801, t0
  ecall

  li      t0, 0x134
  lw      a3, 0(t0)
  li      t0, 0x138
  lw      a1, 0(t0)
  li      t0, 0x13c
  lw      a2, 0(t0)
  divu    t0, a3, t2
  mv      a4, t0
  bltu    a2, a1, scm_fix_max
scm_fix_max_done:
  bltu    a4, a1, scm_fix_avg_lo
scm_fix_avg_lo_done:
  bltu    a2, a4, scm_fix_avg_hi
scm_fix_avg_hi_done:

  la      a0, scm_tag
  jal     ra, print_str
  mv      a0, a1
  jal     ra, print_hex32
  la      a0, sep
  jal     ra, print_str
  mv      a0, a2
  jal     ra, print_hex32
  la      a0, sep
  jal     ra, print_str
  mv      a0, a4
  jal     ra, print_hex32
  la      a0, nl
  jal     ra, print_str

  li      t1, 0x120
  sw      a1, 0(t1)
  sw      a2, 4(t1)
  sw      a4, 8(t1)
  sw      zero, 12(t1)   # workload
  li      t0, 0x134
  lw      t2, 0(t0)
  sw      t2, 16(t1)     # trap roundtrip sum
  la      t0, acc_trap_entry
  lw      t2, 0(t0)
  sw      t2, 20(t1)     # trap entry
  la      t0, acc_trap_body
  lw      t2, 0(t0)
  sw      t2, 24(t1)     # trap body
  sw      zero, 28(t1)   # spill/save
  sw      zero, 32(t1)   # restore
  sw      zero, 36(t1)   # stage
  la      t0, acc_mret_return
  lw      t2, 0(t0)
  sw      t2, 40(t1)     # mret return
  sw      zero, 44(t1)   # cfg mismatch
  j       test_pass

scm_min_upd:
  sw      t2, 0(t0)
  j       scm_min_done

scm_max_upd:
  sw      t2, 0(t0)
  j       scm_max_done

scm_fix_max:
  mv      a2, a1
  j       scm_fix_max_done

scm_fix_avg_lo:
  mv      a4, a1
  j       scm_fix_avg_lo_done

scm_fix_avg_hi:
  mv      a4, a2
  j       scm_fix_avg_hi_done

write_sig_snapshot:
  li      t6, 64
  li      t0, 0x134
  lw      t1, 0(t0)
  li      t0, 0x138
  lw      t2, 0(t0)
  li      t0, 0x13c
  lw      t3, 0(t0)
  divu    t4, t1, t6
  li      t5, 0x120
  sw      t2, 0(t5)
  sw      t3, 4(t5)
  sw      t4, 8(t5)
  sw      zero, 12(t5)
  sw      t1, 16(t5)
  la      t0, acc_trap_entry
  lw      t6, 0(t0)
  sw      t6, 20(t5)
  la      t0, acc_trap_body
  lw      t6, 0(t0)
  sw      t6, 24(t5)
  sw      zero, 28(t5)
  sw      zero, 32(t5)
  sw      zero, 36(t5)
  la      t0, acc_mret_return
  lw      t6, 0(t0)
  sw      t6, 40(t5)
  sw      zero, 44(t5)
  ret

  .align 4
trap_entry:
  csrr    t0, mcause
  beqz    t0, trap_do_mret
  li      t1, 8
  beq     t0, t1, trap_do_mret
  li      t1, 11
  bne     t0, t1, unexpected_trap
trap_do_mret:
  csrr    t2, 0xB00
  la      t3, trap_entry_cycle
  sw      t2, 0(t3)
  csrr    t0, mepc
  addi    t0, t0, 4
  csrw    mepc, t0
  csrr    t2, 0xB00
  la      t3, mret_start_cycle
  sw      t2, 0(t3)
  la      t3, trap_entry_cycle
  lw      t4, 0(t3)
  sub     t5, t2, t4
  la      t3, acc_trap_body
  lw      t4, 0(t3)
  add     t4, t4, t5
  sw      t4, 0(t3)
  mret

unexpected_trap:
  li      t1, 0x10c
  sw      t0, 0(t1)
  csrr    t2, mepc
  sw      t2, 4(t1)
  li      t2, 0xbad00005
  sw      t2, 8(t1)
  j       test_fail

print_str:
  lbu     t0, 0(a0)
  beqz    t0, print_str_done
  li      t1, 0x10000000
  sb      t0, 0(t1)
  addi    a0, a0, 1
  j       print_str
print_str_done:
  ret

print_hex32:
  li      t0, 0x10000000
  li      t1, '0'
  sb      t1, 0(t0)
  li      t1, 'x'
  sb      t1, 0(t0)
  li      t2, 28
  la      t3, hex_tab
scm_hex_loop:
  srl     t4, a0, t2
  andi    t4, t4, 0xf
  add     t4, t4, t3
  lbu     t5, 0(t4)
  sb      t5, 0(t0)
  addi    t2, t2, -4
  bgez    t2, scm_hex_loop
  ret

test_fail:
  jal     ra, write_sig_snapshot
  li      t0, 0
  la      t1, tohost
  sw      t0, 0(t1)
1:
  wfi
  j       1b

mcycle_fail:
  jal     ra, write_sig_snapshot
  li      t0, 0
  li      t1, 0x100
  li      t2, 0xdeadbeef
  sw      t2, 0(t1)
  la      t1, tohost
  sw      t0, 0(t1)
3:
  wfi
  j       3b

test_pass:
  li      t0, 1
  la      t1, tohost
  sw      t0, 0(t1)
2:
  wfi
  j       2b

  .section .tohost, "aw", @progbits
  .align 8
  .globl tohost
tohost:
  .dword 0
  .globl fromhost
fromhost:
  .dword 0

  .section .rodata
hex_tab:
  .asciz "0123456789ABCDEF"
scm_tag:
  .asciz "MCYCLE_SCALE4_MIXED min/max/avg="
sep:
  .asciz "/"
nl:
  .asciz "\n"

  .section .bss
  .align 4
acc_trap_entry:
  .word 0
acc_trap_body:
  .word 0
acc_mret_return:
  .word 0
trap_start_cycle:
  .word 0
trap_entry_cycle:
  .word 0
mret_start_cycle:
  .word 0

  .section .signature, "aw", @progbits
  .align 4
  .globl begin_signature
begin_signature:
  .word 0
  .word 0
  .word 0
  .globl end_signature
end_signature:
  .word 0

