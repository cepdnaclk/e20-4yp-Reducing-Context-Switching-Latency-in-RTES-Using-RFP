// 1. Basic Checks
require_privilege(PRV_M);
set_pc_and_serialize(p->get_state()->mepc->read());

// 2. Prepare MSTATUS (Standard Logic)
reg_t s = STATE.mstatus->read();
reg_t prev_prv = get_field(s, MSTATUS_MPP);
reg_t prev_virt = get_field(s, MSTATUS_MPV);

if (prev_prv != PRV_M)
  s = set_field(s, MSTATUS_MPRV, 0);
s = set_field(s, MSTATUS_MIE, get_field(s, MSTATUS_MPIE));
s = set_field(s, MSTATUS_MPIE, 1);
s = set_field(s, MSTATUS_MPP, p->extension_enabled('U') ? PRV_U : PRV_M);
s = set_field(s, MSTATUS_MPV, 0);

// =========================================================
// CUSTOM AUTO-RESTORE WINDOW LOGIC
// (Moved BEFORE privilege drop to ensure write permission)
// =========================================================

// A. Read the Saved Window Config (from csrw 0x801)
reg_t prev_win_config = p->get_state()->window_staged;

// B. Extract Base and Size
reg_t restore_size = (prev_win_config >> 16) & 0xFFFF;
reg_t restore_base = prev_win_config & 0xFFFF;

// C. Update Hardware State
p->get_state()->window_active = prev_win_config;
p->get_state()->XPR.set_window_config(restore_base, restore_size);

// D. CRITICAL: Update the CSR Map
// We force the write to 0x800 so 'csrr' reads the new value.
if (p->get_state()->csrmap.count(0x800)) {
    p->get_state()->csrmap[0x800]->write(prev_win_config);
    
    // VERIFICATION PRINT: Did the write stick?
    reg_t check = p->get_state()->csrmap[0x800]->read();
    //fprintf(stderr, "[HARDWARE] mret: Win=%lx | CSR[0x800]=%lx\n", 
    //        (unsigned long)prev_win_config, (unsigned long)check);
}

// =========================================================
// RESUME STANDARD LOGIC
// =========================================================

if (ZICFILP_xLPE(prev_virt, prev_prv)) {
  STATE.elp = static_cast<elp_t>(get_field(s, MSTATUS_MPELP));
}
s = set_field(s, MSTATUS_MPELP, elp_t::NO_LP_EXPECTED);
s = set_field(s, MSTATUS_MDT, 0);
if (prev_prv == PRV_U || (prev_virt && prev_prv != PRV_M))
  s = set_field(s, MSTATUS_SDT, 0);
if (prev_virt && prev_prv == PRV_U)
  STATE.vsstatus->write(STATE.vsstatus->read() & ~SSTATUS_SDT);
STATE.mstatus->write(s);
if (STATE.mstatush) STATE.mstatush->write(s >> 32); 
if (STATE.tcontrol) STATE.tcontrol->write((STATE.tcontrol->read() & CSR_TCONTROL_MPTE) ? (CSR_TCONTROL_MPTE | CSR_TCONTROL_MTE) : 0);

// 3. Drop Privilege (Last Step)
p->set_privilege(prev_prv, prev_virt);