set_host_options -max_cores 8

set power_enable_timing_analysis true
set power_analysis_mode averaged

read_verilog gate.v
link_design ${DESIGN}
read_sdc gate.sdc

check_timing
update_timing

read_saif -strip_path ${ANNOTATE_PATH} gate.saif

check_power
update_power
report_power -verbose

exit