# Debugging "s0 lost" and spill-test "all ids 0" with FYP-Modified Spike

This note maps the FreeRTOS port behaviour to your Spike modifications.

**Port fix (done):** If the **first** task to run is windowed (e.g. B), it was started with **ret** after writing 0x801. The FYP hardware applies the staged window only on **mret**. So the first task ran in window 0 until it yielded; after mret it ran in its window, so s0 set before yield was in the wrong partition → "s0 lost". The port now starts a **windowed** first task via **mret** (`xPortStartFirstTaskWindowed`) so the window is applied from the first instruction.

**If your mret already uses `state.window_staged`:** you can still apply the **order fix** below so the window is restored before PC/privilege changes (optional).

---

## mret shows correct base/size but tasks still see wrong ID (e.g. all 0)

If with the dump **disabled** you see **mret base=32, 64, 96, 128, 160, 192, 224** and **base=0** for legacy, then **state.window_staged** and **mret** are correct: the right window is being applied in mret. If the spill test still reports suspension order **0,0,0,...,0** and tasks 1–6 with count=0, then the **next instruction after mret is not using the applied window** when reading GPRs.

So when the task runs and reads **a0** (task id), the CPU must read from the **physical** register that corresponds to the **current window** (e.g. for base=64, logical a0 = x10 → physical 64+10 = 74). If the simulator always reads from physical 10 (or logical index without adding base), every task would see the same value (task 0’s a0) → wrong id.

**What to check in Spike:** When the processor **executes** an instruction that uses a GPR (e.g. source/dest of `add`, `lw`, etc.), does it access the register file using the **current window**? The regfile (XPR) must map **logical** index `i` to **physical** index `(window_base + i)` (or equivalent). So:

- **state.XPR[i]** (or the accessor used by the instruction executor) must return the value in **physical** register `(current_base + i)`, not physical `i`.
- **state.XPR.set_window_config(base, size)** must set the **current** base/size that is used for that mapping.

If your XPR implementation already does this (e.g. `operator[](i)` returns `data[base_offset + i]`), then the bug may be that the **instruction execution path** uses a different regfile or a non-windowed read (e.g. direct `state.XPR.regs[i]` instead of `state.XPR[i]`). Search the Spike code for where GPRs are read during instruction execution and ensure they go through the windowed accessor.

**Trace which id each run sees:** Build `make demo_spill_trace.elf`, run `spike --isa=rv32gc demo_spill_trace.elf 2>&1 | tee spill.log`, then `grep '\[SPILL\]' spill.log` and `grep -o 'id=[0-9]*' spill.log | sort | uniq -c`. See **README.md** section "Spill trace: which task id (a0) does each run see?".

### Interpreting your trace (id=0 only for windowed, 7/8/9 correct)

If you see:

- **`grep -o 'id=[0-9]*' spill.log | sort | uniq -c`** → e.g. `21 id=0`, `15 id=7`, `15 id=8`, `15 id=9`, and **no id=1, id=2, …, id=6**
- **`grep '\[SPILL\]' spill.log`** → long runs of `id=0`, then `id=7`, `id=8`, `id=9` (legacy), then again `id=0`…, never `id=1`…`id=6`

then:

- **Legacy tasks (7, 8, 9)** are correct: they see their own id and run 15 times each.
- **Windowed tasks (1–6)** never see their own id; when they run they see **a0 = 0** (task 0’s id). So every GPR read after mret is still using **window 0** (physical 0–31), not the window mret applied (e.g. base 64 for task 1).
- **Task 0** shows 21 runs because the 6 “runs” that were actually tasks 1–6 (with wrong a0) incremented `counters[0]` instead of `counters[1]`…`counters[6]`.

**Conclusion (port fix, not Spike):** The issue was in the **FreeRTOS port**: on full restore of a windowed task, the port loaded GPRs into the kernel window (phys 0–31) then mret’d to the task window (e.g. 64–95), which was never written — so the task saw wrong a0. **Fix:** After loading the frame, save frame ptr, `csrw 0x800` (task window), reload GPRs from frame, then `csrw 0x801` and mret. See `portContext.h` and `port_restore_frame_ptr` in `port.c`. `set_window_config(base, size)` correctly, but **instruction execution** is not using that window when reading GPRs (e.g. a0). In the Spike build you run, ensure the **same** binary uses the windowed regfile for all integer GPR accesses (see “Reference” below). Rebuild Spike from the tree that has the windowed `regfile_t` and `READ_REG`/`WRITE_REG` via `STATE.XPR`.

**Summary:** With the port fix above, windowed tasks 1–6 see the correct id (a0); Spike and 0x801/mret were already correct.

**Reference (in-repo riscv-isa-sim):** The repo under `FreeRTOS-Kernel/riscv-isa-sim/` has the intended setup:
- **decode.h** – `regfile_t` with `operator[](i)` → `data[i + base_offset]` and `write(i, val)` → `data[i + base_offset]`; `set_window_config(base, size)` sets `base_offset` and `window_size`.
- **decode_macros.h** – `READ_REG(reg)` is `STATE.XPR[reg]`, `WRITE_REG(reg, value)` is `STATE.XPR.write(reg, value)`, so all GPR access goes through the windowed regfile.
- **processor.h** – `state_t` has `regfile_t<reg_t, NXPR, true> XPR` with `NXPR = 256`.

So the **same** Spike binary used for the spill test must be built from this (or an equivalent) tree. If the binary was built from an unmodified Spike (plain array XPR, no window), GPR access will not use the window and the spill test will still show wrong IDs.

---

## CRITICAL: Trap entry must set `state.window_staged` (fix for spill "0,0,0,...,0")

The port’s **minimal save** path does `csrr t0, 0x801` and stores that value in the yielding task’s context so it can be restored later. **get_csr(0x801)** returns **state.window_staged**. So the trap handler must see the **interrupted task’s** window in 0x801.

In your Spike code, **take_trap** currently does:

- `previous_window_config = (current_size << 16) | (current_base & 0xFFFF);`
- `state.XPR.set_window_config(0, 32);`
- It does **not** set **state.window_staged**.

So when the trap handler runs and does `csrr 0x801`, it reads **state.window_staged**, which still holds the value from the **last** `csrw 0x801` (the previously restored task’s window). The **yielding** task’s window was only saved in **previous_window_config**, so the port saves the wrong window into the yielding task’s context. On restore, every task gets the same (or wrong) window → suspension order "0,0,0,...,0" and wrong counts.

**Fix in Spike `take_trap`:** right after saving the current window, set **state.window_staged** so that the trap handler’s read of 0x801 sees the yielding task’s config:

```c
// In take_trap, right after:
previous_window_config = (current_size << 16) | (current_base & 0xFFFF);

// ADD THIS so the trap handler’s csrr 0x801 gets the yielding task’s window:
state.window_staged = previous_window_config;

// Then force kernel window:
state.XPR.set_window_config(0, 32);
```

Then:

1. Trap handler’s minimal save does `csrr t0, 0x801` → gets the yielding task’s window → stores it in that task’s frame.
2. When that task is later restored, the port loads that value and does `csrw 0x801` → **state.window_staged** = that task’s window.
3. **mret** reads **state.window_staged** and applies it → task runs in the correct partition.

Without this, **mret** is correct but the **saved** context is wrong, so all windowed tasks behave as if they had the same (or incorrect) window.

## Expected data flow

| Step | Who | Action |
|------|-----|--------|
| 1 | Task B (windowed) | Runs in window (base=32, size=32). Sets s0 = MAGIC, then yields (trap). |
| 2 | **take_trap** | Saves current window to `previous_window_config` = (32<<16)\|32; **must also set state.window_staged** = same value; forces XPR to (0, 32). |
| 3 | Port (save) | Reads **CSR 0x801** → `state.window_staged` (yielding task’s window, e.g. 0x00200020 for B). Takes minimal save path, stores it on B's stack. |
| 4 | Scheduler | Picks task A, restores A (full path). Port writes **0** to 0x801 so mret applies window 0. **mret** → A runs in window (0, 32). |
| 5 | Task A | Runs, yields. take_trap saves (0,32) to previous_window_config, forces window (0,32). |
| 6 | Port (restore B) | Minimal restore: loads 0x00200020 from B's stack, **csrw 0x801, t0** → **put_csr(0x801, 0x00200020)** sets **state.window_staged = 0x00200020**. Then **mret**. |
| 7 | **mret** | Must read **state.window_staged** (0x00200020), set XPR to (base=32, size=32), then return to B's PC. |
| 8 | Task B | Resumes; logical x8 (s0) must map to **physical 40**, which still holds MAGIC → "s0 persisted". |

**Port fix (full restore):** When restoring a **legacy** task the port used to skip writing 0x801, so `state.window_staged` kept the previous task's window and mret applied the wrong partition. The port now **always** writes 0x801 on restore: **0** for legacy (so mret applies window 0), and the task's `window_cfg` for windowed tasks.

If s0 is lost, step 7 is wrong: after mret the CPU is not in window (32, 32), so B still sees window 0 and s0 = kernel’s x8.

## Critical: mret must use `state.window_staged`, not `previous_window_config`

- **state.window_staged** = value **staged by software** with `csrw 0x801` (the *next* task’s window). This is what mret must apply.
- **previous_window_config** = value **set by take_trap** (the *yielding* task’s window). Used only for debugging/read of 0x801 in your CSR class; must **not** be used in mret.

In your `mret.h` you must have:

```c
reg_t prev_win_config = p->get_state()->window_staged;   // correct
```

If instead you have:

```c
reg_t prev_win_config = p->previous_window_config;      // WRONG
```

then when restoring B, mret would apply the *yielding* task (A)’s window (0, 32), so B would resume in window 0 and s0 would be wrong → "s0 lost".

**Fix in Spike:** In the mret handler, ensure the staged config is read from **state.window_staged** (the value written by `csrw 0x801`), not from `previous_window_config`.

## put_csr(0x801, val) behaviour — **CRITICAL: do not clear window_staged**

Your `put_csr` for 0x801 does:

1. `state.window_staged = val`
2. Dump (and restore XPR to saved kernel window)
3. `return`

So the value that mret must use is **state.window_staged**. When the CPU executes **csrw 0x801, rs1**, Spike may call **csrmap[0x801]->write(val)** (the CSR object’s `write`), not necessarily `put_csr`. Your **prev_window_config_csr_t::write(val)** only sets **previous_window_config**; it does **not** set **state.window_staged**. So mret always reads **state.window_staged** and gets 0 (or a stale value). **Fix:** In **prev_window_config_csr_t::write(val)** also set **proc->get_state()->window_staged = val** so that whichever path (put_csr or csrmap->write) is used, mret sees the correct value.

### If [mret] base=0 size=0 every time

Then **state.window_staged is 0 when mret runs**. The port has already done `csrw 0x801, t0` with the task's window_cfg (e.g. 0x00200020) just before `mret`, so something in the simulator is clearing or overwriting **state.window_staged** between that write and mret.

**Cause:** In **put_csr(0x801, val)** (or in the CSR write path for 0x801), after you set `state.window_staged = val`, you likely do a “restore kernel window” step that switches the visible XPR to (0, 32). If that step (or any code it calls) also sets `state.window_staged = 0` or `state.window_staged = (32<<16)|0` or similar, then the value the port wrote is lost. When the trap handler later executes `mret`, it reads `state.window_staged` and gets 0.

**Fix:** In **put_csr(0x801, val)** (and anywhere else that touches the window):

1. Set `state.window_staged = val` (so mret will apply the task’s window later).
2. Do whatever you need for the trap handler (e.g. switch visible window to kernel with `set_window_config(0, 32)`).
3. **Do not** assign to `state.window_staged` again. No “clear staging”, no “set to current window”, no overwrite. It must stay equal to `val` until the next `csrw 0x801` or until mret reads it.

Search your Spike code for every assignment to `state.window_staged` (or `window_staged`). The only places that should set it are:

- **take_trap:** `state.window_staged = previous_window_config` (so the trap handler’s `csrr 0x801` sees the yielding task’s window).
- **put_csr(0x801, val):** `state.window_staged = val` (so mret later sees the next task’s window). No second assignment after that.

### If you have a HARDWARE DUMP in put_csr(0x801)

If **put_csr(0x801, val)** does `state.window_staged = val` and then runs a **HARDWARE DUMP** loop that calls **state.XPR.set_window_config(...)** to peek at physical banks, then **set_window_config** (or the XPR/regfile implementation) may have a **side effect** that sets **state.window_staged** to the “current” window (e.g. 0 or (32<<16)|0). That would overwrite the value you just set, so when **mret** runs it reads 0 → base=0 size=0.

**Fix:** In **put_csr(0x801, val)** save and restore **state.window_staged** around the dump so it is still **val** when put_csr returns (and when mret later runs). You should see mret bases 32, 64, …, 224 for windowed tasks and 0 for legacy; that is correct.

**Correct pattern:** `saved_staged = state.window_staged` right after `state.window_staged = val`, run the dump, then **state.XPR.set_window_config(saved_base, saved_size)** and **state.window_staged = saved_staged** before `return`.

```c
if (which == 0x801) {
    state.window_staged = val;
    reg_t saved_staged = state.window_staged;   // backup (== val)
    reg_t saved_base = state.XPR.get_base_offset();
    reg_t saved_size = state.XPR.get_window_size();
    // ... your dump loop (set_window_config, etc.) ...
    state.XPR.set_window_config(saved_base, saved_size);
    state.window_staged = saved_staged;         // restore so mret sees val
    return;
}
```

Alternatively, **disable the dump for normal runs** so you can confirm the spill test passes without it.

---

## Quick fix: disable HARDWARE DUMP when testing spill

To verify that the spill test passes and **window_staged** is correct, temporarily **turn off** the HARDWARE DUMP in **put_csr(0x801)** so it never runs on context switch. Then **state.window_staged** cannot be overwritten by the dump.

In **processor.cc**, in **put_csr**, for the `which == 0x801` block, do only:

```c
if (which == 0x801) {
    state.window_staged = val;
    return;
}
```

Comment out or `#if 0` the entire dump block (fprintf, loop, set_window_config, etc.). Rebuild Spike and run the spill test. If you then see correct suspension order (e.g. 0,1,2,...,9 or mixed) and correct counts for all 10 tasks, the bug is confirmed to be the dump (or something after put_csr) overwriting **window_staged**. You can then either:

- Keep the dump disabled for normal runs, or  
- Re-enable the dump and add **state.window_staged = saved_staged** at the end of the 0x801 block (after the dump, before `return`), with `saved_staged = state.window_staged` right after `state.window_staged = val`.

### If mret still shows base=0 size=0 even with restore

1. **Check what value the port writes:** In **put_csr(0x801)**, right after `state.window_staged = val;`, add:
   `fprintf(stderr, "[put_csr 0x801] val=0x%lx base=%lu\n", (unsigned long)val, (unsigned long)(val & 0xFFFF));`
   Run the spill test. If you see **val=0** (or base=0) every time, the **port** is loading 0 from the task’s saved frame (wrong task or wrong offset). If you see **val=0x200020, 0x200040**, etc., then the value is correct and something **after** put_csr (e.g. **csrmap[0x801]->write(val)**) is overwriting **state.window_staged** before mret.

2. **Ensure the CSR object also sets window_staged:** When **csrw 0x801** is executed, Spike may call **both** put_csr(0x801, val) **and** csrmap[0x801]->write(val). If **prev_window_config_csr_t::write(val)** does not set **proc->get_state()->window_staged = val**, then after put_csr returns (with window_staged correct), the second call can leave window_staged unchanged or set to the wrong value. So in **prev_window_config_csr_t::write(val)** add:
   `proc->get_state()->window_staged = val;`
   and make **read()** return **proc->get_state()->window_staged** so mret and the port see the same staging value.

## Optional verification in Spike

In `mret.h`, after applying the window, assert or print:

```c
reg_t restore_base = prev_win_config & 0xFFFF;
// After: p->get_state()->XPR.set_window_config(restore_base, restore_size);
assert(p->get_state()->XPR.get_base_offset() == restore_base && "mret must apply 0x801 to XPR");
```

And confirm that `prev_win_config` is read from `p->get_state()->window_staged`, not from `p->previous_window_config`.

## Summary

- FreeRTOS port: minimal restore path does `csrw 0x801, <task's window_cfg>` immediately before `mret`, matching your FYP test pattern.
- Spike: **mret** must apply **state.window_staged** (the value from that `csrw 0x801`) to the register window. If mret uses **previous_window_config** instead, the returning task stays in window 0 and you get "s0 lost".
- Fix: In mret, use `p->get_state()->window_staged` as the staged config to apply to XPR.

---

## Order fix (when mret already uses window_staged) — **REQUIRED for spill test**

Your current mret.h does **set_pc_and_serialize(mepc) first**, then the CUSTOM window logic. That is wrong: the **next** instruction must run with the **new** window, so the window must be applied **before** the PC is changed. Move the **entire** "CUSTOM AUTO-RESTORE WINDOW LOGIC" block to the **very top** of mret, before `require_privilege` and before `set_pc_and_serialize`:

```c
// mret.h — correct order

// 1. Apply staged register window FIRST (before PC or privilege change)
reg_t prev_win_config = p->get_state()->window_staged;
reg_t restore_size = (prev_win_config >> 16) & 0xFFFF;
reg_t restore_base = prev_win_config & 0xFFFF;
if (restore_size == 0) restore_size = 32;
p->get_state()->window_active = prev_win_config;
p->get_state()->XPR.set_window_config(restore_base, restore_size);
if (p->get_state()->csrmap.count(0x800))
  p->get_state()->csrmap[0x800]->write(prev_win_config);

// 2. Basic checks and PC
require_privilege(PRV_M);
set_pc_and_serialize(p->get_state()->mepc->read());

// 3. Prepare MSTATUS (standard logic)
reg_t s = STATE.mstatus->read();
// ... rest of mret (prev_prv, prev_virt, mstatus updates, set_privilege)
```

Reason: if the PC is updated (or privilege dropped) before the window is applied, the simulator may commit state or expose the new PC with the old window still active, so the first instruction after mret could see the wrong registers. Applying the window first guarantees the next instruction runs with the correct partition.

Optional debug: right after reading `prev_win_config`, add  
`fprintf(stderr, "[mret] prev_win_config=0x%lx base=%lu size=%lu\n", (unsigned long)prev_win_config, (unsigned long)restore_base, (unsigned long)restore_size);`  
When restoring task B you should see `prev_win_config=0x200020 base=32 size=32`. If you see `base=0 size=32`, something cleared `window_staged` before mret (e.g. in `put_csr` or elsewhere).

---

## Status: Legacy OK, windowed 1–6 still see ID 0

If the spill test shows suspension order like `0,0,0,0,0,0,0,9,7,8` and Task 7/8/9 have count=15 while Tasks 1–6 have count=0, then:

- **Legacy path is correct:** writing 0 to 0x801 on restore and applying window (0, 32) on mret works; tasks 7–9 see their own ID.
- **Windowed tasks 1–6** are still running in task 0’s partition: when they run, they read `a0` (task ID) from window 0, so they see ID 0. So either:
  1. **Port** is not writing each task’s distinct `window_cfg` (32, 64, 96, …) to 0x801 when restoring tasks 1–6, or
  2. **Simulator mret** is not applying a non-zero base (e.g. only applies when `prev_win_config == 0`), or
  3. **set_window_config(base, size)** for base=64,96,… is not switching the visible GPR bank (e.g. logical x10 still reads physical p10 instead of p42).

**Next step:** In **mret**, right after computing `restore_base` and `restore_size`, add:
`fprintf(stderr, "[mret] base=%lu size=%lu\n", (unsigned long)restore_base, (unsigned long)restore_size);`
Run the spill test and inspect stderr. You should see a mix of `base=0` (legacy), `base=32`, `base=64`, …, `base=224`.  
**If you see `base=0 size=0` every time:** `state.window_staged` is being cleared after the port writes 0x801. See the section **"put_csr(0x801) — do not clear window_staged"** and **"If [mret] base=0 size=0 every time"** below: fix **put_csr(0x801)** so it never overwrites `state.window_staged` after setting it to `val`.  
If you see 0, 32, 64, 96, … but tasks 1–6 still report ID 0, the bug is in **set_window_config** or in how the processor uses the window for the next instruction.

---

## If spill still shows 0,0,0 after take_trap fix

If you added `state.window_staged = previous_window_config` in `take_trap` and rebuilt Spike but the spill test still reports suspension order `0,0,0,...,0` and wrong counts:

### 1. Confirm you are running the rebuilt Spike

- Run `which spike` (or your full path). Use the spike binary from your build directory, e.g. `./spike` from the Spike build dir or `$SPIKE_BUILD/spike`.
- If your shell uses a system or other `spike` first, you may still be running the old binary.

### 2. Confirm take_trap change is in the binary

- In `take_trap`, add temporarily:  
  `fprintf(stderr, "[take_trap] window_staged=0x%lx\n", (unsigned long)state.window_staged);`  
  right after `state.window_staged = previous_window_config;`
- Run the spill test. You should see many `[take_trap] window_staged=0x...` lines with **different** values (e.g. 0x200020, 0x200040, 0x200060, …). If you never see this line, the rebuilt Spike is not being used.

### 3. Confirm mret sees different window configs

- In **mret** (e.g. in `insns/mret.h` or wherever mret is implemented), right after reading `prev_win_config`, add:  
  `fprintf(stderr, "[mret] base=%lu\n", (unsigned long)(prev_win_config & 0xFFFF));`
- Run the spill test. You should see **mret base=32, 64, 96, 128, 160, 192, 224** (and possibly 0 for legacy) in a mixed order. If you only ever see `base=32` or `base=0`, then either:
  - The port is writing the same value to 0x801 every time (wrong), or
  - `put_csr(0x801)` is overwriting or not preserving `state.window_staged` before mret runs.

### 4. Apply window before PC/privilege in mret

- The **next** instruction after mret must run with the **new** window. In your mret handler, apply the window (read `window_staged`, call `XPR.set_window_config(restore_base, restore_size)`) **before** `set_pc_and_serialize(mepc)` and before dropping privilege. See the “Order fix” section above. If the window is applied only after the PC is changed, the first instruction after mret may still be executed with the old window.

### 5. put_csr(0x801) must not clear window_staged

- Your `put_csr(0x801, val)` sets `state.window_staged = val`, then does the dump and at the end does `state.XPR.set_window_config(saved_base, saved_size)`. It must **not** set `state.window_staged` to 0 or anything else after that. The value written by the port must still be in `state.window_staged` when the **mret** instruction is executed (right after the port returns from the code that called put_csr).
