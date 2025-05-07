DW_SIM_DIR=/usr/cad/synopsys/synthesis/cur/dw/sim_ver
TECH_SIM=/usr/cad/cell_lib/TN40G/CBDK_TSMC40_Arm_f2.0/CIC/Verilog/sc9_cln40g_base_rvt.v
TECH_SIM_NEG=/usr/cad/cell_lib/TN40G/CBDK_TSMC40_Arm_f2.0/CIC/Verilog/sc9_cln40g_base_rvt_neg.v
TECH_SIM_UDP=/usr/cad/cell_lib/TN40G/CBDK_TSMC40_Arm_f2.0/CIC/Verilog/sc9_cln40g_base_rvt_udp.v

TOP=../src/trv.sv
DESIGN=trv
GATE_TOP=tb_system
ANNOTATE_PATH=/${GATE_TOP}/sim_rtcore_inst/DUT/gen_trv[0]/trv_inst

error:
	@echo "Please choose one of the following target: c_sim, lint, rtl_sim, syn, gate_sim, pwr"
	@exit 1

c_sim:
	g++ -DDUMP_TRACE -Wall -Wno-unknown-pragmas -O3 -isystem/usr/cad/mentor/Catapult/cur/Mgc_home/shared/include ../test/tb_rtcore.cpp ../src/rtcore.cpp -o tb_rtcore
	g++ -Wall -O3 ../util/shuffle_test_vectors.cpp -o shuffle_test_vectors
	./tb_rtcore
	./shuffle_test_vectors init_req.bin 44 trv_req.bin 56
	./shuffle_test_vectors bbox_req.bin 92 bbox_resp.bin 27
	./shuffle_test_vectors ist_req.bin 68 ist_resp.bin 13

lint:
	verilator -Wall --lint-only --timing -I../src -I../test ../test/tb_system.sv

rtl_sim:
	vlog -64 +libext+.v -y ${DW_SIM_DIR} ../src/concat_rtl.v
	vlog -64 ../src/fifo_bw.sv
	vlog -64 ../src/fifo_fw.sv
	vlog -64 ../src/fifo_multi.sv
	vlog -64 ../src/fifo.sv
	vlog -64 ../src/reorder.sv
	vlog -64 ../src/rtcore_top.sv
	vlog -64 ../src/sram_1r1w.sv
	vlog -64 ../src/trv.sv
	vlog -64 ../test/tb_system.sv
	vopt -64 tb_system -o tb_system_opt
	vsim -64 tb_system_opt

syn:
	dcnxt_shell -f syn.tcl -x "set TOP ${TOP}; set DESIGN ${DESIGN}" -output_log_file syn.log

gate_sim:
	vlog -64 +libext+.v -y ${DW_SIM_DIR} ../src/concat_rtl.v
	vlog -64 ../src/fifo_bw.sv
	vlog -64 ../src/fifo_fw.sv
	vlog -64 ../src/fifo_multi.sv
	vlog -64 ../src/fifo.sv
	vlog -64 ../src/reorder.sv
	vlog -64 ../src/rtcore_top.sv
	vlog -64 ../src/sram_1r1w.sv
	vlog -64 -v ${TECH_SIM} -v ${TECH_SIM_NEG} -v ${TECH_SIM_UDP} gate.v
	vlog -64 ../test/${GATE_TOP}.sv
	vopt -64 +acc=npr+${ANNOTATE_PATH}. -sdftyp ${ANNOTATE_PATH}=gate.sdf +notimingchecks ${GATE_TOP} -o tb_opt
	vsim -64 -sdfnoerror -c -onfinish stop -do "power add -r ${ANNOTATE_PATH}/*; run -all; power report -all -bsaif gate.saif; exit" tb_opt
	# run the following commands in QuestaSim console
	# run <timesteps>
	# power report -all -bsaif gate.saif

pwr:
	pwr_shell -f pwr.tcl -x "set DESIGN ${DESIGN}; set ANNOTATE_PATH ${ANNOTATE_PATH}" -output_log_file pwr.log

clean:
	rm -rf tb_rtcore shuffle_test_vectors trace.txt *.bin
	rm -rf transcript work *.wlf etch*
	rm -rf *.svf *.log *.ddc *.v *.sdc *.sdf alib-*
	rm -rf .rce *.fsdb *.saif primetime_px*
