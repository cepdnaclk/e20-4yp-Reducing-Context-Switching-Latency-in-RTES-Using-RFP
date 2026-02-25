// Read-only CSR class allowing user-level reads, disallowing writes
class ctx_ro_csr_t final : public basic_csr_t {
 public:
  ctx_ro_csr_t(processor_t* const proc, reg_t addr)
    : basic_csr_t(proc, addr, 0) {}
  void verify_permissions(insn_t insn, bool write) const override {
    if (write)
      throw trap_illegal_instruction(insn.bits());
  }
};

// CSR registration
add_csr(CSR_MCTX_START, mctx_start = std::make_shared<ctx_ro_csr_t>(proc, CSR_MCTX_START));
add_csr(CSR_MCTX_END,   mctx_end   = std::make_shared<ctx_ro_csr_t>(proc, CSR_MCTX_END));
add_csr(CSR_MCTX_DELTA, mctx_delta = std::make_shared<ctx_ro_csr_t>(proc, CSR_MCTX_DELTA));
