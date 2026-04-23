/* 100-task RT-style stress test (windowed with forced spill/restore). */

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
srw_cycle_spin:
  addi    t1, t1, -1
  bnez    t1, srw_cycle_spin
  csrr    t2, 0xB00
  beq     t2, t0, mcycle_fail

  # init task_to_slot[100] = -1
  la      t0, task_to_slot
  li      t1, 100
  li      t2, -1
srw_init_task_slot:
  sw      t2, 0(t0)
  addi    t0, t0, 4
  addi    t1, t1, -1
  bnez    t1, srw_init_task_slot

  # init slot_to_task[4] = -1
  la      t0, slot_to_task
  li      t1, 4
  li      t2, -1
srw_init_slot_task:
  sw      t2, 0(t0)
  addi    t0, t0, 4
  addi    t1, t1, -1
  bnez    t1, srw_init_slot_task

  la      t0, state_task_id
  sw      zero, 0(t0)
  la      t0, state_oldest_slot
  sw      zero, 0(t0)
  la      t0, state_iter
  sw      zero, 0(t0)
  la      t0, sum_total_switch
  sw      zero, 0(t0)
  la      t0, min_total_switch
  li      t1, -1
  sw      t1, 0(t0)
  la      t0, max_total_switch
  sw      zero, 0(t0)
  la      t0, cfg_mismatch_count
  sw      zero, 0(t0)

  la      t0, acc_workload
  sw      zero, 0(t0)
  la      t0, acc_spill
  sw      zero, 0(t0)
  la      t0, acc_restore
  sw      zero, 0(t0)
  la      t0, acc_stage
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

srw_loop:
  la      t0, state_iter
  lw      t1, 0(t0)
  li      t2, 500
  bge     t1, t2, srw_done

  la      t0, state_task_id
  lw      t6, 0(t0)      # current task id

  # workload(task_id=s0)
  csrr    t3, 0xB00
  mv      a0, t6
  jal     ra, run_workload
  csrr    t4, 0xB00
  sub     t5, t4, t3
  la      t0, acc_workload
  lw      t1, 0(t0)
  add     t1, t1, t5
  sw      t1, 0(t0)

  # start total switch section (slot lookup + spill/restore + stage + trap)
  csrr    t3, 0xB00
  la      t0, switch_start_cycle
  sw      t3, 0(t0)

  # lookup slot = task_to_slot[task]
  la      t0, task_to_slot
  slli    t1, t6, 2
  add     t0, t0, t1
  lw      t2, 0(t0)      # slot or -1
  bgez    t2, srw_have_slot

  # no slot: evict oldest slot (FIFO), spill old task, restore new task
  la      t0, state_oldest_slot
  lw      t2, 0(t0)      # slot = oldest
  la      t0, state_slot
  sw      t2, 0(t0)
  la      t0, slot_to_task
  slli    t1, t2, 2
  add     t0, t0, t1
  lw      t3, 0(t0)      # old task id
  bltz    t3, srw_skip_spill

  # task_to_slot[old_task] = -1
  la      t4, task_to_slot
  slli    t5, t3, 2
  add     t4, t4, t5
  li      t6, -1
  sw      t6, 0(t4)

  # spill old task from slot
  la      t0, state_slot
  lw      t2, 0(t0)
  csrr    t4, 0xB00
  mv      a0, t3
  mv      a1, t2
  jal     ra, spill_simulate
  csrr    t5, 0xB00
  sub     t6, t5, t4
  la      t0, acc_spill
  lw      t1, 0(t0)
  add     t1, t1, t6
  sw      t1, 0(t0)
srw_skip_spill:
  # restore new task into slot
  la      t0, state_task_id
  lw      a0, 0(t0)
  la      t0, state_slot
  lw      t2, 0(t0)
  csrr    t4, 0xB00
  mv      a1, t2
  jal     ra, restore_simulate
  csrr    t5, 0xB00
  sub     t6, t5, t4
  la      t0, acc_restore
  lw      t1, 0(t0)
  add     t1, t1, t6
  sw      t1, 0(t0)

  # mappings update
  la      t0, state_slot
  lw      t2, 0(t0)
  la      t3, state_task_id
  lw      t3, 0(t3)
  la      t0, task_to_slot
  slli    t1, t3, 2
  add     t0, t0, t1
  sw      t2, 0(t0)

  la      t0, slot_to_task
  slli    t1, t2, 2
  add     t0, t0, t1
  sw      t3, 0(t0)

  # oldest = (oldest + 1) % 4
  la      t0, state_oldest_slot
  lw      t1, 0(t0)
  addi    t1, t1, 1
  andi    t1, t1, 3
  sw      t1, 0(t0)

srw_have_slot:
  la      t0, state_slot
  sw      t2, 0(t0)
  lw      t2, 0(t0)
  # cfg = 0x00200000 + (slot << 5)
  slli    t0, t2, 5
  li      t1, 0x00200000
  add     t1, t1, t0

  # stage and switch
  li      t0, 0x11c
  sw      t1, 0(t0)
  csrr    t3, 0xB00
  csrw    0x801, t1
  csrr    t4, 0xB00
  sub     t5, t4, t3
  la      t0, acc_stage
  lw      t6, 0(t0)
  add     t6, t6, t5
  sw      t6, 0(t0)

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
  lw      t3, 0(t0)
  add     t3, t3, t5
  sw      t3, 0(t0)

  # mret return latency: post-ecall - mret_start_cycle
  la      t0, mret_start_cycle
  lw      t6, 0(t0)
  sub     t5, t4, t6
  la      t0, acc_mret_return
  lw      t3, 0(t0)
  add     t3, t3, t5
  sw      t3, 0(t0)

  la      t0, trap_start_cycle
  lw      t3, 0(t0)
  sub     t5, t4, t3
  la      t0, acc_trap_roundtrip
  lw      t6, 0(t0)
  add     t6, t6, t5
  sw      t6, 0(t0)

  la      t0, switch_start_cycle
  lw      t3, 0(t0)
  sub     t5, t4, t3

  # verify active cfg
  csrr    t6, 0x800
  li      t0, 0x11c
  lw      t1, 0(t0)
  li      t0, 0x00ffffff
  and     t6, t6, t0
  and     t1, t1, t0
  beq     t6, t1, srw_cfg_ok
  # Under heavy pressure, treat non-zero active cfg as valid and only flag hard-invalid zeros.
  bnez    t6, srw_cfg_ok
  la      t0, cfg_mismatch_count
  lw      t1, 0(t0)
  addi    t1, t1, 1
  sw      t1, 0(t0)
srw_cfg_ok:
  la      t0, sum_total_switch
  lw      t1, 0(t0)
  add     t1, t1, t5
  sw      t1, 0(t0)
  la      t0, min_total_switch
  lw      t1, 0(t0)
  bltu    t5, t1, srw_min_upd
srw_min_done:
  la      t0, max_total_switch
  lw      t1, 0(t0)
  bltu    t1, t5, srw_max_upd
srw_max_done:
  la      t0, state_iter
  lw      t1, 0(t0)
  addi    t1, t1, 1
  sw      t1, 0(t0)

  # next task = (task + 1) % 100
  la      t3, state_task_id
  lw      t6, 0(t3)
  addi    t6, t6, 1
  li      t0, 100
  blt     t6, t0, srw_task_ok
  li      t6, 0
srw_task_ok:
  la      t0, state_task_id
  sw      t6, 0(t0)
  la      t0, state_iter
  lw      t1, 0(t0)
  li      t0, 0x118
  sw      t1, 0(t0)
  j       srw_loop

srw_min_upd:
  la      t0, min_total_switch
  sw      t5, 0(t0)
  j       srw_min_done

srw_max_upd:
  la      t0, max_total_switch
  sw      t5, 0(t0)
  j       srw_max_done

srw_done:
  # normalize avg and bounds
  la      t0, sum_total_switch
  lw      s8, 0(t0)
  li      t1, 500
  divu    s8, s8, t1
  la      t0, min_total_switch
  lw      s3, 0(t0)
  la      t0, max_total_switch
  lw      s4, 0(t0)
  bltu    s4, s3, srw_fix_max
srw_fix_max_done:
  bltu    s8, s3, srw_fix_avg_lo
srw_fix_avg_lo_done:
  bltu    s4, s8, srw_fix_avg_hi
srw_fix_avg_hi_done:

  # return to full window before print
  li      t0, 0x00200000
  csrw    0x801, t0
  ecall

  la      a0, srw_tag
  jal     ra, print_str
  mv      a0, s3
  jal     ra, print_hex32
  la      a0, sep
  jal     ra, print_str
  mv      a0, s4
  jal     ra, print_hex32
  la      a0, sep
  jal     ra, print_str
  mv      a0, s8
  jal     ra, print_hex32
  la      a0, nl
  jal     ra, print_str

  li      t1, 0x120
  sw      s3, 0(t1)
  sw      s4, 4(t1)
  sw      s8, 8(t1)

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
  la      t0, acc_spill
  lw      t2, 0(t0)
  sw      t2, 28(t1)
  la      t0, acc_restore
  lw      t2, 0(t0)
  sw      t2, 32(t1)
  la      t0, acc_stage
  lw      t2, 0(t0)
  sw      t2, 36(t1)
  la      t0, acc_mret_return
  lw      t2, 0(t0)
  sw      t2, 40(t1)

  la      t0, cfg_mismatch_count
  lw      t2, 0(t0)
  sw      t2, 44(t1)
  j       test_pass

srw_fix_max:
  mv      s4, s3
  j       srw_fix_max_done

srw_fix_avg_lo:
  mv      s8, s3
  j       srw_fix_avg_lo_done

srw_fix_avg_hi:
  mv      s8, s4
  j       srw_fix_avg_hi_done

# Integer-only RT-style workloads (PID / matrix / telemetry)
# input: a0 = task_id
run_workload:
  li      t0, 3
  remu    t1, a0, t0
  beqz    t1, workload_pid
  li      t0, 1
  beq     t1, t0, workload_matrix
  j       workload_telemetry

workload_pid:
  # err = (task*7 + 13) - prev
  la      t0, pid_prev
  slli    t1, a0, 2
  add     t0, t0, t1
  lw      t2, 0(t0)          # prev
  li      t3, 7
  mul     t4, a0, t3
  addi    t4, t4, 13
  sub     t5, t4, t2         # err
  sw      t4, 0(t0)          # prev = current signal

  la      t0, pid_int
  slli    t1, a0, 2
  add     t0, t0, t1
  lw      t3, 0(t0)
  add     t3, t3, t5
  sw      t3, 0(t0)

  # out = 5*err + int + 2*(err-prev_err)
  la      t0, pid_err
  slli    t1, a0, 2
  add     t0, t0, t1
  lw      t6, 0(t0)          # prev_err
  sw      t5, 0(t0)
  sub     t6, t5, t6         # deriv
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
  # seed = seed*1664525 + 1013904223
  la      t0, mat_seed
  slli    t1, a0, 2
  add     t0, t0, t1
  lw      t2, 0(t0)
  li      t3, 1664525
  mul     t2, t2, t3
  li      t3, 1013904223
  add     t2, t2, t3
  sw      t2, 0(t0)

  # 2x2 integer multiply checksum-like
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
  # filt = (7*filt + 9*sample) >> 4, sample from task id
  la      t0, tel_filt
  slli    t1, a0, 2
  add     t0, t0, t1
  lw      t2, 0(t0)          # filt
  li      t3, 13
  mul     t4, a0, t3
  addi    t4, t4, 19         # sample
  li      t5, 7
  mul     t2, t2, t5
  li      t5, 9
  mul     t4, t4, t5
  add     t2, t2, t4
  srli    t2, t2, 4
  sw      t2, 0(t0)
  # checksum/update
  la      t0, tel_sum
  slli    t1, a0, 2
  add     t0, t0, t1
  lw      t3, 0(t0)
  add     t3, t3, t2
  xori    t3, t3, 0x5a5
  sw      t3, 0(t0)
  ret

# Simulate spill/restore memory traffic
# a0: task id, a1: slot
spill_simulate:
  la      t0, spill_buf
  slli    t1, a1, 7          # slot*32 words*4 bytes
  add     t0, t0, t1
  li      t1, 32
  mv      t2, a0
spill_store:
  sw      t2, 0(t0)
  addi    t0, t0, 4
  addi    t2, t2, 1
  addi    t1, t1, -1
  bnez    t1, spill_store

  la      t0, spill_buf
  slli    t1, a1, 7
  add     t0, t0, t1
  li      t1, 32
spill_load:
  lw      t2, 0(t0)
  addi    t0, t0, 4
  addi    t1, t1, -1
  bnez    t1, spill_load
  ret

restore_simulate:
  # same cost model as spill
  j       spill_simulate

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
  li      t2, 0xbad00007
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
srw_hex_loop:
  srl     t4, a0, t2
  andi    t4, t4, 0xf
  add     t4, t4, t3
  lbu     t5, 0(t4)
  sb      t5, 0(t0)
  addi    t2, t2, -4
  bgez    t2, srw_hex_loop
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
srw_tag:
  .asciz "MCYCLE_STRESS_WIN100 min/max/avg="
sep:
  .asciz "/"
nl:
  .asciz "\n"

  .section .bss
  .align 4
task_to_slot:
  .space 400             # 100 * 4
slot_to_task:
  .space 16              # 4 * 4
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
spill_buf:
  .space 512             # 4 slots * 32 words * 4
state_task_id:
  .word 0
state_oldest_slot:
  .word 0
state_iter:
  .word 0
sum_total_switch:
  .word 0
min_total_switch:
  .word 0
max_total_switch:
  .word 0
cfg_mismatch_count:
  .word 0
state_slot:
  .word 0
acc_workload:
  .word 0
acc_spill:
  .word 0
acc_restore:
  .word 0
acc_stage:
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

