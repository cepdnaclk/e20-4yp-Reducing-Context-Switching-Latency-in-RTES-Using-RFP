/* 100-task RT-style stress baseline (unpartitioned + software context save/restore). */

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
srb_cycle_spin:
  addi    t1, t1, -1
  bnez    t1, srb_cycle_spin
  csrr    t2, 0xB00
  beq     t2, t0, mcycle_fail

  li      s0, 0          # task id
  li      s1, 500        # iterations
  li      s2, 0          # sum
  li      s3, -1         # min
  li      s4, 0          # max
  li      s5, 0          # iter
  li      s6, 0          # avg

  la      t0, acc_workload
  sw      zero, 0(t0)
  la      t0, acc_save_restore
  sw      zero, 0(t0)
  la      t0, acc_trap_roundtrip
  sw      zero, 0(t0)
  la      t0, acc_trap_entry
  sw      zero, 0(t0)
  la      t0, acc_trap_body
  sw      zero, 0(t0)
  la      t0, acc_mret_return
  sw      zero, 0(t0)

  li      t0, 0x118
  sw      zero, 0(t0)

srb_loop:
  bge     s5, s1, srb_done

  csrr    t3, 0xB00
  mv      a0, s0
  jal     ra, run_workload
  csrr    t4, 0xB00
  sub     t5, t4, t3
  la      t0, acc_workload
  lw      t1, 0(t0)
  add     t1, t1, t5
  sw      t1, 0(t0)

  csrr    t3, 0xB00
  la      t0, switch_start_cycle
  sw      t3, 0(t0)

  csrr    t3, 0xB00
  jal     ra, sw_context_save_restore
  csrr    t4, 0xB00
  sub     t5, t4, t3
  la      t0, acc_save_restore
  lw      t1, 0(t0)
  add     t1, t1, t5
  sw      t1, 0(t0)

  csrr    t3, 0xB00
  la      t0, trap_start_cycle
  sw      t3, 0(t0)
  ecall
  csrr    t4, 0xB00

  # interrupt entry latency: trap_entry_cycle - trap_start_cycle
  la      t0, trap_entry_cycle
  lw      t6, 0(t0)
  la      t0, trap_start_cycle
  lw      t3, 0(t0)
  sub     t5, t6, t3
  la      t0, acc_trap_entry
  lw      t1, 0(t0)
  add     t1, t1, t5
  sw      t1, 0(t0)

  # mret return latency: post-ecall - mret_start_cycle
  la      t0, mret_start_cycle
  lw      t6, 0(t0)
  sub     t5, t4, t6
  la      t0, acc_mret_return
  lw      t1, 0(t0)
  add     t1, t1, t5
  sw      t1, 0(t0)

  la      t0, trap_start_cycle
  lw      t3, 0(t0)
  sub     t5, t4, t3
  la      t0, acc_trap_roundtrip
  lw      t1, 0(t0)
  add     t1, t1, t5
  sw      t1, 0(t0)

  la      t0, switch_start_cycle
  lw      t3, 0(t0)
  sub     t2, t4, t3

  add     s2, s2, t2
  bltu    t2, s3, srb_min_upd
srb_min_done:
  bltu    s4, t2, srb_max_upd
srb_max_done:
  addi    s5, s5, 1

  addi    s0, s0, 1
  li      t0, 100
  blt     s0, t0, srb_task_ok
  li      s0, 0
srb_task_ok:
  li      t0, 0x118
  sw      s5, 0(t0)
  j       srb_loop

srb_min_upd:
  mv      s3, t2
  j       srb_min_done

srb_max_upd:
  mv      s4, t2
  j       srb_max_done

srb_done:
  divu    s6, s2, s1
  bltu    s4, s3, srb_fix_max
srb_fix_max_done:
  bltu    s6, s3, srb_fix_avg_lo
srb_fix_avg_lo_done:
  bltu    s4, s6, srb_fix_avg_hi
srb_fix_avg_hi_done:

  la      a0, srb_tag
  jal     ra, print_str
  mv      a0, s3
  jal     ra, print_hex32
  la      a0, sep
  jal     ra, print_str
  mv      a0, s4
  jal     ra, print_hex32
  la      a0, sep
  jal     ra, print_str
  mv      a0, s6
  jal     ra, print_hex32
  la      a0, nl
  jal     ra, print_str

  li      t1, 0x120
  sw      s3, 0(t1)
  sw      s4, 4(t1)
  sw      s6, 8(t1)
  la      t0, acc_workload
  lw      t2, 0(t0)
  sw      t2, 12(t1)
  la      t0, acc_trap_roundtrip
  lw      t2, 0(t0)
  sw      t2, 16(t1)
  la      t0, acc_trap_entry
  lw      t2, 0(t0)
  sw      t2, 20(t1)
  la      t0, acc_trap_body
  lw      t2, 0(t0)
  sw      t2, 24(t1)
  la      t0, acc_save_restore
  lw      t2, 0(t0)
  sw      t2, 28(t1)
  sw      zero, 32(t1)
  sw      zero, 36(t1)
  la      t0, acc_mret_return
  lw      t2, 0(t0)
  sw      t2, 40(t1)
  sw      zero, 44(t1)
  j       test_pass

srb_fix_max:
  mv      s4, s3
  j       srb_fix_max_done

srb_fix_avg_lo:
  mv      s6, s3
  j       srb_fix_avg_lo_done

srb_fix_avg_hi:
  mv      s6, s4
  j       srb_fix_avg_hi_done

sw_context_save_restore:
  addi    sp, sp, -128
  li      t0, 32
  mv      t1, sp
srb_store_loop:
  sw      t0, 0(t1)
  addi    t1, t1, 4
  addi    t0, t0, -1
  bnez    t0, srb_store_loop

  li      t0, 32
  mv      t1, sp
srb_load_loop:
  lw      t2, 0(t1)
  addi    t1, t1, 4
  addi    t0, t0, -1
  bnez    t0, srb_load_loop
  addi    sp, sp, 128
  ret

# Integer-only RT-style workloads (same classes as windowed test)
run_workload:
  li      t0, 3
  remu    t1, a0, t0
  beqz    t1, workload_pid
  li      t0, 1
  beq     t1, t0, workload_matrix
  j       workload_telemetry

workload_pid:
  la      t0, pid_prev
  slli    t1, a0, 2
  add     t0, t0, t1
  lw      t2, 0(t0)
  li      t3, 7
  mul     t4, a0, t3
  addi    t4, t4, 13
  sub     t5, t4, t2
  sw      t4, 0(t0)

  la      t0, pid_int
  slli    t1, a0, 2
  add     t0, t0, t1
  lw      t3, 0(t0)
  add     t3, t3, t5
  sw      t3, 0(t0)

  la      t0, pid_err
  slli    t1, a0, 2
  add     t0, t0, t1
  lw      t6, 0(t0)
  sw      t5, 0(t0)
  sub     t6, t5, t6
  li      t0, 5
  mul     t0, t5, t0
  slli    t6, t6, 1
  add     t0, t0, t3
  add     t0, t0, t6
  la      t1, pid_out
  slli    t2, a0, 2
  add     t1, t1, t2
  sw      t0, 0(t1)
  ret

workload_matrix:
  la      t0, mat_seed
  slli    t1, a0, 2
  add     t0, t0, t1
  lw      t2, 0(t0)
  li      t3, 1664525
  mul     t2, t2, t3
  li      t3, 1013904223
  add     t2, t2, t3
  sw      t2, 0(t0)

  andi    t3, t2, 0xff
  srli    t4, t2, 8
  andi    t4, t4, 0xff
  srli    t5, t2, 16
  andi    t5, t5, 0xff
  srli    t6, t2, 24
  andi    t6, t6, 0xff
  mul     t0, t3, t3
  mul     t1, t4, t5
  add     t0, t0, t1
  mul     t1, t3, t4
  mul     t2, t4, t6
  add     t1, t1, t2
  xor     t0, t0, t1
  la      t1, mat_out
  slli    t2, a0, 2
  add     t1, t1, t2
  sw      t0, 0(t1)
  ret

workload_telemetry:
  la      t0, tel_filt
  slli    t1, a0, 2
  add     t0, t0, t1
  lw      t2, 0(t0)
  li      t3, 13
  mul     t4, a0, t3
  addi    t4, t4, 19
  li      t5, 7
  mul     t2, t2, t5
  li      t5, 9
  mul     t4, t4, t5
  add     t2, t2, t4
  srli    t2, t2, 4
  sw      t2, 0(t0)
  la      t0, tel_sum
  slli    t1, a0, 2
  add     t0, t0, t1
  lw      t3, 0(t0)
  add     t3, t3, t2
  xori    t3, t3, 0x5a5
  sw      t3, 0(t0)
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
  li      t2, 0xbad00008
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
srb_hex_loop:
  srl     t4, a0, t2
  andi    t4, t4, 0xf
  add     t4, t4, t3
  lbu     t5, 0(t4)
  sb      t5, 0(t0)
  addi    t2, t2, -4
  bgez    t2, srb_hex_loop
  ret

test_fail:
  li      t0, 0
  la      t1, tohost
  sw      t0, 0(t1)
1:
  wfi
  j       1b

mcycle_fail:
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
srb_tag:
  .asciz "MCYCLE_STRESS_BASE100 min/max/avg="
sep:
  .asciz "/"
nl:
  .asciz "\n"

  .section .bss
  .align 4
pid_prev:
  .space 400
pid_err:
  .space 400
pid_int:
  .space 400
pid_out:
  .space 400
mat_seed:
  .space 400
mat_out:
  .space 400
tel_filt:
  .space 400
tel_sum:
  .space 400
acc_workload:
  .word 0
acc_save_restore:
  .word 0
acc_trap_roundtrip:
  .word 0
acc_trap_entry:
  .word 0
acc_trap_body:
  .word 0
acc_mret_return:
  .word 0
switch_start_cycle:
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

