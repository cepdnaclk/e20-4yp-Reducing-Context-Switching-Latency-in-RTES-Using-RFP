/* Baseline trap latency test with software save/restore simulation. */

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
lb_cycle_spin:
  addi    t1, t1, -1
  bnez    t1, lb_cycle_spin
  csrr    t2, 0xB00
  beq     t2, t0, mcycle_fail

  li      s0, 0
  li      s1, 128
  li      s2, 0
  li      s3, -1
  li      s4, 0
  li      s9, 0x118
  sw      zero, 0(s9)
  la      t0, acc_trap_entry
  sw      zero, 0(t0)
  la      t0, acc_trap_body
  sw      zero, 0(t0)
  la      t0, acc_mret_return
  sw      zero, 0(t0)

baseline_loop:
  call    dummy_save_restore
  csrr    t3, 0xB00
  la      t0, trap_start_cycle
  sw      t3, 0(t0)
  csrr    t3, 0xB00
  ecall
  csrr    t4, 0xB00
  la      t0, trap_start_cycle
  lw      t3, 0(t0)
  sub     t2, t4, t3
  la      t0, trap_entry_cycle
  lw      t5, 0(t0)
  la      t0, trap_start_cycle
  lw      t6, 0(t0)
  sub     t5, t5, t6
  la      t0, acc_trap_entry
  lw      t6, 0(t0)
  add     t6, t6, t5
  sw      t6, 0(t0)
  la      t0, mret_start_cycle
  lw      t5, 0(t0)
  sub     t5, t4, t5
  la      t0, acc_mret_return
  lw      t6, 0(t0)
  add     t6, t6, t5
  sw      t6, 0(t0)
  add     s2, s2, t2
  bltu    t2, s3, lb_min_upd
lb_min_done:
  bltu    s4, t2, lb_max_upd
lb_max_done:
  addi    s0, s0, 1
  sw      s0, 0(s9)
  blt     s0, s1, baseline_loop

  divu    t0, s2, s1
  mv      s5, t0
  bltu    s4, s3, lb_fix_max
lb_fix_max_done:
  bltu    s5, s3, lb_fix_avg_lo
lb_fix_avg_lo_done:
  bltu    s4, s5, lb_fix_avg_hi
lb_fix_avg_hi_done:

  la      a0, lb_tag
  jal     ra, print_str
  mv      a0, s3
  jal     ra, print_hex32
  la      a0, sep
  jal     ra, print_str
  mv      a0, s4
  jal     ra, print_hex32
  la      a0, sep
  jal     ra, print_str
  mv      a0, s5
  jal     ra, print_hex32
  la      a0, nl
  jal     ra, print_str

  li      t1, 0x120
  sw      s3, 0(t1)
  sw      s4, 4(t1)
  sw      s5, 8(t1)
  sw      zero, 12(t1)   # workload
  sw      s2, 16(t1)     # trap roundtrip sum
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

lb_min_upd:
  mv      s3, t2
  j       lb_min_done

lb_max_upd:
  mv      s4, t2
  j       lb_max_done

lb_fix_max:
  mv      s4, s3
  j       lb_fix_max_done

lb_fix_avg_lo:
  mv      s5, s3
  j       lb_fix_avg_lo_done

lb_fix_avg_hi:
  mv      s5, s4
  j       lb_fix_avg_hi_done

write_sig_snapshot:
  li      t6, 128
  divu    t4, s2, t6
  li      t5, 0x120
  sw      s3, 0(t5)
  sw      s4, 4(t5)
  sw      t4, 8(t5)
  sw      zero, 12(t5)
  sw      s2, 16(t5)
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

dummy_save_restore:
  addi    sp, sp, -64
  sw      t0, 0(sp)
  sw      t1, 4(sp)
  sw      t2, 8(sp)
  sw      t3, 12(sp)
  sw      t4, 16(sp)
  sw      t5, 20(sp)
  sw      t6, 24(sp)
  sw      a0, 28(sp)
  sw      a1, 32(sp)
  sw      a2, 36(sp)
  sw      a3, 40(sp)
  sw      a4, 44(sp)
  sw      a5, 48(sp)
  sw      a6, 52(sp)
  sw      a7, 56(sp)
  sw      ra, 60(sp)
  lw      t0, 0(sp)
  lw      t1, 4(sp)
  lw      t2, 8(sp)
  lw      t3, 12(sp)
  lw      t4, 16(sp)
  lw      t5, 20(sp)
  lw      t6, 24(sp)
  lw      a0, 28(sp)
  lw      a1, 32(sp)
  lw      a2, 36(sp)
  lw      a3, 40(sp)
  lw      a4, 44(sp)
  lw      a5, 48(sp)
  lw      a6, 52(sp)
  lw      a7, 56(sp)
  lw      ra, 60(sp)
  addi    sp, sp, 64
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
  li      t2, 0xbad00004
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
lb_hex_loop:
  srl     t4, a0, t2
  andi    t4, t4, 0xf
  add     t4, t4, t3
  lbu     t5, 0(t4)
  sb      t5, 0(t0)
  addi    t2, t2, -4
  bgez    t2, lb_hex_loop
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
lb_tag:
  .asciz "MCYCLE_LAT_SW min/max/avg="
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

