# CVA6 Out-of-Context (OOC) Core Synthesis Script for Area Comparison
# Targets reference FPGA platform (Digilent Genesys 2 Kintex-7 by default)
#
# Usage (in batch mode):
#   vivado -nojournal -mode batch -source synth_core_ooc.tcl

set part "xc7k325tffg900-2"
if { [info exists ::env(XILINX_PART)] } {
    set part $::env(XILINX_PART)
}
set top_module "cva6"
if { [info exists ::env(TOP_MODULE)] } {
    set top_module $::env(TOP_MODULE)
}
set clock_period_ns 20.0
if { [info exists ::env(CLK_PERIOD_NS)] } {
    set clock_period_ns $::env(CLK_PERIOD_NS)
}
set rfp_enabled 1
if { [info exists ::env(RFP_ENABLED)] } {
    set rfp_enabled $::env(RFP_ENABLED)
}
set build_dir "./ooc_work_${top_module}_rfp${rfp_enabled}"

puts "==================================================================="
puts " CVA6 FPGA OOC Synthesis"
puts " Target Part:    $part (Genesys 2 Reference: xc7k325tffg900-2)"
puts " Top Module:     $top_module"
puts " Clock Period:   $clock_period_ns ns"
puts " RFP Enabled:    $rfp_enabled"
puts " Build Dir:      $build_dir"
puts "==================================================================="

create_project -force cva6_ooc_synth $build_dir -part $part

# Set defines for RFP/DRFP configuration
if { $rfp_enabled == 1 } {
    set_property verilog_define {FPGA_TARGET=1 DRFP_ENABLED=1 RFP_ENABLED=1} [current_fileset]
} else {
    set_property verilog_define {FPGA_TARGET=1 DRFP_ENABLED=0 RFP_ENABLED=0} [current_fileset]
}

# Include directories (relative to cva6/verif/tests/custom/context_switch/fpga/)
set repo_root "../../../../../"
set_property include_dirs [list \
    "${repo_root}/core/include" \
    "${repo_root}/vendor/pulp-platform/common_cells/include" \
    "${repo_root}/vendor/pulp-platform/axi/include" \
    "${repo_root}/core/cache_subsystem/hpdcache/rtl/include" \
    "${repo_root}/corev_apu/register_interface/include" \
    "${repo_root}/corev_apu/instr_tracing/ITI/include" \
] [current_fileset]

# Source generated source list from repo root or fallback
set sources_tcl "${repo_root}/corev_apu/fpga/scripts/add_sources.tcl"
if { [file exists $sources_tcl] } {
    puts "INFO: Reading RTL sources from $sources_tcl"
    source $sources_tcl
} elseif { [file exists "add_sources.tcl"] } {
    puts "INFO: Reading RTL sources from local add_sources.tcl"
    source "add_sources.tcl"
} else {
    puts "ERROR: add_sources.tcl not found. Please generate sources first via 'make fpga' or run_area_comparison.sh."
    exit 1
}

# Set Top Module
set_property top $top_module [current_fileset]
update_compile_order -fileset sources_1

# Create OOC constraints (virtual clock for timing driven synthesis)
set xdc_file "${build_dir}/ooc_timing.xdc"
set xdc_fp [open $xdc_file w]
puts $xdc_fp "create_clock -name clk_i -period $clock_period_ns \[get_ports clk_i\]"
close $xdc_fp
add_files -fileset constrs_1 -norecurse $xdc_file
set_property target_constrs_file $xdc_file [current_fileset -constrs_1]
set_property USED_IN {synthesis implementation out_of_context} [get_files $xdc_file]

# Synthesize Design (Out-Of-Context mode)
puts "INFO: Launching OOC Synthesis..."
synth_design -top $top_module -part $part -mode out_of_context -retiming

# Create reports directory
set reports_dir "reports_rfp${rfp_enabled}"
file mkdir $reports_dir

puts "INFO: Writing synthesis reports to $reports_dir..."
check_timing -verbose -file ${reports_dir}/${top_module}_check_timing.rpt
report_timing_summary -delay_type max -max_paths 10 -file ${reports_dir}/${top_module}_timing_summary.rpt
report_utilization -hierarchical -file ${reports_dir}/${top_module}_utilization_hierarchical.rpt
report_utilization -file ${reports_dir}/${top_module}_utilization.rpt

puts "INFO: OOC Synthesis and Reporting completed successfully."
exit 0
