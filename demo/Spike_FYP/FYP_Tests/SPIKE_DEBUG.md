# Debugging "s0 lost" with FYP-Modified Spike

This note maps the FreeRTOS port behaviour to your Spike modifications.

**Port fix (done):** If the **first** task to run is windowed (e.g. B), it was started with **ret** after writing 0x801. The FYP hardware applies the staged window only on **mret**. So the first task ran in window 0 until it yielded; after mret it ran in its window, so s0 set before yield was in the wrong partition → "s0 lost". The port now starts a **windowed** first task via **mret** (`xPortStartFirstTaskWindowed`) so the window is applied from the first instruction.

**If your mret already uses `state.window_staged`:** you can still apply the **order fix** below so the window is restored before PC/privilege changes (optional).

## Expected data flow

| Step | Who | Action |
|------|-----|--------|
| 1 | Task B (windowed) | Runs in window (base=32, size=32). Sets s0 = MAGIC, then yields (trap). |
| 2 | **take_trap** | Saves current window to `previous_window_config` = (32<<16)\|32; forces XPR to (0, 32). |
| 3 | Port (save) | Reads **CSR 0x801** → `state.window_staged` (still 0x00200020 from last restore of B). Takes minimal save path, stores 0x00200020 on B's stack. |
| 4 | Scheduler | Picks task A, restores A (full path). No write to 0x801. **mret** → A runs in window (0, 32). |
| 5 | Task A | Runs, yields. take_trap saves (0,32) to previous_window_config, forces window (0,32). |
| 6 | Port (restore B) | Minimal restore: loads 0x00200020 from B's stack, **csrw 0x801, t0** → **put_csr(0x801, 0x00200020)** sets **state.window_staged = 0x00200020**. Then **mret**. |
| 7 | **mret** | Must read **state.window_staged** (0x00200020), set XPR to (base=32, size=32), then return to B's PC. |
| 8 | Task B | Resumes; logical x8 (s0) must map to **physical 40**, which still holds MAGIC → "s0 persisted". |

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

## put_csr(0x801, val) behaviour

Your `put_csr` for 0x801 does:

1. `state.window_staged = val`
2. Dump (and restore XPR to saved kernel window)
3. `return` (so **csrmap[0x801]->write(val) is never called**)

So the value that mret must use is **state.window_staged**. The `prev_window_config_csr_t` in csrmap[0x801] is only used for **read** (get_csr returns state.window_staged in your snippet); its write() is never invoked for 0x801. That is consistent as long as mret uses **state.window_staged**.

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

## Order fix (when mret already uses window_staged)

If you already have `prev_win_config = p->get_state()->window_staged` but still see "s0 lost", the register window must be applied **before** any other mret side effects (PC change, mstatus, privilege). Move the **entire** "CUSTOM AUTO-RESTORE WINDOW LOGIC" block to the **very top** of mret, before `set_pc_and_serialize` and before any mstatus reads:

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
