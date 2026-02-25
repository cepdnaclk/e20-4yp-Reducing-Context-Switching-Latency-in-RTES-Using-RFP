// On sret, capture end and delta
if (STATE.mctx_end && STATE.mcycle) {
  const reg_t end_cycle = STATE.mcycle->read();
  STATE.mctx_end->write(end_cycle);
  const reg_t start_cycle = STATE.mctx_start ? STATE.mctx_start->read() : 0;
  const reg_t delta = end_cycle >= start_cycle ? end_cycle - start_cycle : 0;
  if (STATE.mctx_delta)
    STATE.mctx_delta->write(delta);
}
