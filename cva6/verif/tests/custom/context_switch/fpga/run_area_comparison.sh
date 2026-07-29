#!/usr/bin/env bash
# ==============================================================================
# CVA6 FPGA Area & Frequency Benchmarking Harness
# Evaluates Baseline (32 regs) vs DRFP Partitioned (64 regs) on Xilinx Vivado
# Reference Platform: Digilent Genesys 2 Kintex-7 (xc7k325tffg900-2)
# ==============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../../../../" &> /dev/null && pwd)"

# Default settings
export XILINX_PART="${XILINX_PART:-xc7k325tffg900-2}"
export TOP_MODULE="${TOP_MODULE:-cva6}"
export CLK_PERIOD_NS="${CLK_PERIOD_NS:-20.0}"
export BOARD="${BOARD:-genesys2}"

echo "=============================================================================="
echo " CVA6 FPGA Comparative Area & Timing Benchmarking Harness"
echo " Project Root:  ${REPO_ROOT}"
echo " FPGA Board:    ${BOARD} (Part: ${XILINX_PART})"
echo " Top Module:    ${TOP_MODULE}"
echo " Target Clock:  ${CLK_PERIOD_NS} ns"
echo "=============================================================================="

# Check if Vivado is installed in PATH
if ! command -v vivado &> /dev/null; then
    echo ""
    echo "[WARNING] 'vivado' command not found in PATH."
    echo "This script requires Xilinx Vivado (2018.3+ or 2020.2+ recommended) to run synthesis."
    echo "Please execute this script on a workstation or build server with Vivado installed:"
    echo "  cd ${SCRIPT_DIR}"
    echo "  bash run_area_comparison.sh"
    echo ""
    exit 0
fi

# Ensure add_sources.tcl is generated in corev_apu/fpga/scripts/
SOURCES_TCL="${REPO_ROOT}/corev_apu/fpga/scripts/add_sources.tcl"
if [ ! -f "${SOURCES_TCL}" ]; then
    echo "[FPGA_HARNESS] Generating RTL source list (${SOURCES_TCL})..."
    mkdir -p "${REPO_ROOT}/corev_apu/fpga/scripts/"
    
    # Run Makefile target or generate manually using flist_flattener
    cd "${REPO_ROOT}"
    if make -n fpga &> /dev/null; then
        # Use make dry-run or generate sources via make
        make -B corev_apu/fpga/scripts/add_sources.tcl 2>/dev/null || true
    fi
    
    if [ ! -s "${SOURCES_TCL}" ]; then
        echo "[FPGA_HARNESS] Generating add_sources.tcl via python flist_flattener..."
        python3 "${REPO_ROOT}/util/flist_flattener.py" "${REPO_ROOT}/core/Flist.cva6" | grep "\.sv" | sed 's/^/read_verilog -sv {/' | sed 's/$/}/' > "${SOURCES_TCL}"
    fi
    cd "${SCRIPT_DIR}"
fi

echo ""
echo "[FPGA_HARNESS] --- Step 1: Synthesizing Baseline CVA6 (RFP_ENABLED=0, 32 Regs) ---"
export RFP_ENABLED=0
vivado -nojournal -nolog -mode batch -source synth_core_ooc.tcl

echo ""
echo "[FPGA_HARNESS] --- Step 2: Synthesizing DRFP Partitioned CVA6 (RFP_ENABLED=1, 64 Regs) ---"
export RFP_ENABLED=1
vivado -nojournal -nolog -mode batch -source synth_core_ooc.tcl

echo ""
echo "[FPGA_HARNESS] --- Step 3: Parsing Utilization & Timing Reports ---"
if command -v python3 &> /dev/null; then
    python3 parse_area_reports.py --baseline reports_rfp0/ --drfp reports_rfp1/ --output area_comparison_table.md
    echo ""
    cat area_comparison_table.md
else
    echo "[WARNING] python3 not found. Reports are saved in reports_rfp0/ and reports_rfp1/."
fi

echo ""
echo "[FPGA_HARNESS] Benchmarking campaign complete! Results stored in ${SCRIPT_DIR}/area_comparison_table.md"
exit 0
