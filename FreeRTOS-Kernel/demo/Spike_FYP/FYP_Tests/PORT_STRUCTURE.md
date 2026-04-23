# Port structure: portASM.S vs portContext.h

## Do NOT add the restore/window block to portASM.S

The block that:
- loads `window_cfg` from the saved frame and does `csrw 0x801` for full restore,
- and the minimal restore path (label 3, `csrw portasmCSR_WINDOW_STAGED`, `mret`),

lives in **portContext.h** inside the **`portcontextRESTORE_CONTEXT`** macro. It must **not** be pasted into portASM.S.

## How it works

1. **portASM.S** includes portContext.h at the top: `#include "portContext.h"`.
2. **portASM.S** defines the trap handler and other entry points, and in the trap handler it **invokes** the macro: `portcontextRESTORE_CONTEXT`.
3. When the assembler processes portASM.S, it **expands** that macro by pulling in the macro body from portContext.h. So the restore code (including the 0x801 write for windowed tasks) is generated in place of `portcontextRESTORE_CONTEXT`.

## If you get undefined reference to freertos_risc_v_trap_handler, pxPortInitialiseStack, xPortStartFirstTask

Those symbols are defined in **portASM.S** only. If you pasted the restore block into portASM.S and overwrote or removed the rest of the file, the linker will report those undefined references.

**Fix:** Restore **portASM.S** so that it:

1. Keeps `#include "portContext.h"` near the top.
2. Keeps the `.global` lines for `xPortStartFirstTask`, `pxPortInitialiseStack`, `freertos_risc_v_trap_handler`, etc.
3. Keeps the full `pxPortInitialiseStack` function (and the rest of the file up to and including the trap handler).
4. Keeps the trap handler that ends with `portcontextRESTORE_CONTEXT` (the macro call, not the macro body).
5. Does **not** contain a duplicate copy of the restore macro body (no inline `addi t4, sp, -portCONTEXT_SIZE` … `mret` block).

Use the version of **portASM.S** from this repo (or your last known-good copy) and do **not** add the contents of the `portcontextRESTORE_CONTEXT` macro into portASM.S. Ensure **portContext.h** has the full macro with the window 0x801 logic; then portASM.S only needs to call the macro.
