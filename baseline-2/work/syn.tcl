set CYCLE 2.0

set_host_options -max_cores 8

define_design_lib "WORK" -path "work"
analyze -format sverilog $TOP
elaborate $DESIGN

set_operating_conditions -library sc9_cln40g_base_rvt_tt_typical_max_0p90v_25c tt_typical_max_0p90v_25c
set_load 0.00633 [all_outputs]
set_driving_cell -no_design_rule -library sc9_cln40g_base_rvt_tt_typical_max_0p90v_25c -lib_cell DFFQ_X0P5M_A9TR -pin Q [remove_from_collection [all_inputs] [get_ports {clk arst_n}]]

create_clock -name clk -period $CYCLE [get_ports {clk}]
set_clock_transition 0.01 [get_clocks {clk}] 
set_input_delay -clock [get_clocks {clk}] 0.0 [remove_from_collection [all_inputs] [get_ports {clk arst_n}]]
set_input_transition 0.01 [remove_from_collection [all_inputs] [get_ports {clk}]]
set_output_delay -clock [get_clocks {clk}] 0.0 [all_outputs]

list_libs
check_design
check_timing

# When using compile_ultra, hold violation will not be fixed even if set_fix_hold is on
# To avoid this problem, use compile instead of compile_ultra

# set_fix_hold [all_clocks]
# compile

set_clock_gating_style -positive_edge_logic and:AND2_X0P5B_A9TR
compile_ultra -gate_clock
set_annotated_delay 0.0 -cell -to [get_pins -hierarchical "main_gate/Y"]

report_design
report_resources -hierarchy
report_area -hierarchy -designware
report_timing
report_qor

write -hierarchy -format ddc -output gate.ddc
write -hierarchy -format verilog -output gate.v
write_sdc gate.sdc
write_sdf gate.sdf

exit