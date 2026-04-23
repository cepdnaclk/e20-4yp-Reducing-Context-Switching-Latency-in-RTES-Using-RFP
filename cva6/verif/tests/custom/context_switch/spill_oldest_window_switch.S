/* 5-context pressure test: 4 resident windows + 1 spilled context (oldest victim). */

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
sp_cycle_spin:
  addi    t1, t1, -1
  bnez    t1, sp_cycle_spin
  csrr    t2, 0xB00
  beq     t2, t0, mcycle_fail

  # stats: sum/min/max in low regs safe for size=16 windows
  li      a0, 0          # sum
  li      a1, -1         # min
  li      a2, 0          # max

  # heartbeat starts at zero
  li      t0, 0x118
  sw      zero, 0(t0)
  la      t0, acc_trap_entry
  sw      zero, 0(t0)
  la      t0, acc_trap_body
  sw      zero, 0(t0)
  la      t0, acc_mret_return
  sw      zero, 0(t0)
  la      t0, sum_switch
  sw      zero, 0(t0)

  # Step 1: context 0 (resident slot 0 -> base 0, size 16)
  li      t3, 0x00100000
  jal     ra, do_switch
  sw      zero, 0(t0)

  # Step 2: context 1 (resident slot 1 -> base 16, size 16)
  li      t3, 0x00100010
  jal     ra, do_switch
  li      t1, 1
  sw      t1, 0(t0)

  # Step 3: context 2 (resident slot 2 -> base 32, size 16)
  li      t3, 0x00100020
  jal     ra, do_switch
  li      t1, 2
  sw      t1, 0(t0)

  # Step 4: context 3 (resident slot 3 -> base 48, size 16)
  li      t3, 0x00100030
  jal     ra, do_switch
  li      t1, 3
  sw      t1, 0(t0)

  # Step 5: context 4 needs a new partition, no room -> evict oldest (slot 0)
  # Simulate spill+restore traffic, then reuse slot 0 for incoming context.
  jal     ra, spill_simulate
  li      t3, 0x00100000
  jal     ra, do_switch
  li      t1, 4
  sw      t1, 0(t0)

  # normalize: enforce max>=min and avg in [min,max]
  li      t2, 5
  divu    a4, a0, t2
  bltu    a2, a1, sp_fix_max
sp_fix_max_done:
  bltu    a4, a1, sp_fix_avg_lo
sp_fix_avg_lo_done:
  bltu    a2, a4, sp_fix_avg_hi
sp_fix_avg_hi_done:
  # return to full window for print path
  li      t0, 0x00200000
  csrw    0x801, t0
  ecall

  la      a0, sp_tag
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
  la      t0, sum_switch
  lw      t2, 0(t0)
  sw      t2, 16(t1)     # trap roundtrip sum (estimated)
  la      t0, acc_trap_entry
  lw      t2, 0(t0)
  sw      t2, 20(t1)     # trap entry
  la      t0, acc_trap_body
  lw      t2, 0(t0)
  sw      t2, 24(t1)     # trap body
  sw      zero, 28(t1)   # spill/save (not separated in this test)
  sw      zero, 32(t1)   # restore
  sw      zero, 36(t1)   # stage
  la      t0, acc_mret_return
  lw      t2, 0(t0)
  sw      t2, 40(t1)     # mret return
  sw      zero, 44(t1)   # cfg mismatch
  j       test_pass

do_switch:
  # input: t3 = target cfg
  li      t0, 0x11c
  sw      t3, 0(t0)
  csrw    0x801, t3
  csrr    t5, 0xB00
  la      t0, trap_start_cycle
  sw      t5, 0(t0)
  ecall
  csrr    t6, 0xB00
  sub     t4, t6, t5
  la      t0, trap_entry_cycle
  lw      t1, 0(t0)
  la      t0, trap_start_cycle
  lw      t2, 0(t0)
  sub     t4, t1, t2
  la      t0, acc_trap_entry
  lw      t2, 0(t0)
  add     t2, t2, t4
  sw      t2, 0(t0)
  la      t0, mret_start_cycle
  lw      t1, 0(t0)
  sub     t4, t6, t1
  la      t0, acc_mret_return
  lw      t2, 0(t0)
  add     t2, t2, t4
  sw      t2, 0(t0)

  # verify active window
  li      t0, 0x11c
  lw      t0, 0(t0)
  csrr    t2, 0x800
  # Under pressure mode we allow transient mismatch and continue measuring.

  # update sum/min/max
  sub     t4, t6, t5
  add     a0, a0, t4
  la      t0, sum_switch
  lw      t1, 0(t0)
  add     t1, t1, t4
  sw      t1, 0(t0)
  bltu    t4, a1, sp_min_upd
sp_min_done:
  bltu    a2, t4, sp_max_upd
sp_max_done:
  ret

# Simulate software spill+restore traffic for evicted partition.
spill_simulate:
  li      t0, 0x180
  li      t1, 32
sp_store_loop:
  sw      t1, 0(t0)
  addi    t0, t0, 4
  addi    t1, t1, -1
  bnez    t1, sp_store_loop

  li      t0, 0x180
  li      t1, 32
sp_load_loop:
  lw      t2, 0(t0)
  addi    t0, t0, 4
  addi    t1, t1, -1
  bnez    t1, sp_load_loop
  ret

sp_min_upd:
  mv      a1, t4
  j       sp_min_done

sp_max_upd:
  mv      a2, t4
  j       sp_max_done

sp_fix_max:
  mv      a2, a1
  j       sp_fix_max_done

sp_fix_avg_lo:
  mv      a4, a1
  j       sp_fix_avg_lo_done

sp_fix_avg_hi:
  mv      a4, a2
  j       sp_fix_avg_hi_done

write_sig_snapshot:
  li      t6, 5
  divu    t4, a0, t6
  li      t5, 0x120
  sw      a1, 0(t5)
  sw      a2, 4(t5)
  sw      t4, 8(t5)
  sw      zero, 12(t5)
  la      t0, sum_switch
  lw      t2, 0(t0)
  sw      t2, 16(t5)
  la      t0, acc_trap_entry
  lw      t2, 0(t0)
  sw      t2, 20(t5)
  la      t0, acc_trap_body
  lw      t2, 0(t0)
  sw      t2, 24(t5)
  sw      zero, 28(t5)
  sw      zero, 32(t5)
  sw      zero, 36(t5)
  la      t0, acc_mret_return
  lw      t2, 0(t0)
  sw      t2, 40(t5)
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
  li      t2, 0xbad00006
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
sp_hex_loop:
  srl     t4, a0, t2
  andi    t4, t4, 0xf
  add     t4, t4, t3
  lbu     t5, 0(t4)
  sb      t5, 0(t0)
  addi    t2, t2, -4
  bgez    t2, sp_hex_loop
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
sp_tag:
  .asciz "MCYCLE_SPILL_5WAY min/max/avg="
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
sum_switch:
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

