// In take_trap(), on trap entry
if (state.mctx_start && state.mcycle)
  state.mctx_start->write(state.mcycle->read());
