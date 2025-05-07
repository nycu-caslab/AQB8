`timescale 1ns / 1ps
`include "../include/datatypes.svh"

`define CYCLE_TIME 2.0
`define N_TESTCASES 100000

module tb_init;

// input
logic                       clk;
logic                       arst_n;
logic [`INIT_REQ_WIDTH-1:0] init_req_stream_rsc_dat;
logic                       init_req_stream_rsc_vld;
logic                       trv_req_stream_rsc_rdy;

// output
logic                       init_req_stream_rsc_rdy;
logic [ `TRV_REQ_WIDTH-1:0] trv_req_stream_rsc_dat;
logic                       trv_req_stream_rsc_vld;

init DUT (
    .clk(clk),
    .arst_n(arst_n),
    .init_req_stream_rsc_dat(init_req_stream_rsc_dat),
    .init_req_stream_rsc_vld(init_req_stream_rsc_vld),
    .init_req_stream_rsc_rdy(init_req_stream_rsc_rdy),
    .trv_req_stream_rsc_dat(trv_req_stream_rsc_dat),
    .trv_req_stream_rsc_vld(trv_req_stream_rsc_vld),
    .trv_req_stream_rsc_rdy(trv_req_stream_rsc_rdy)
);

//////////////////////////////////////////////////

initial begin
    clk                     = 0;
    init_req_stream_rsc_dat = 0;
    init_req_stream_rsc_vld = 1;
    trv_req_stream_rsc_rdy  = 1;

    force clk = 0;
    arst_n = 1;
    #(`CYCLE_TIME);
    arst_n = 0;
    #(`CYCLE_TIME);
    arst_n = 1;
    #(`CYCLE_TIME);
    release clk;
end

// verilator lint_off BLKSEQ
always #(`CYCLE_TIME/2.0) clk = ~clk;
// verilator lint_on BLKSEQ

//////////////////////////////////////////////////

// verilator lint_off INITIALDLY
initial begin
    int init_req_fd;

    init_req_fd = $fopen("init_req.bin", "rb");
    assert(init_req_fd != 0);

    forever begin
        bit [11*32-1:0] init_req;

        if ($fread(init_req, init_req_fd) == 0)
            break;
        init_req = {<<byte{init_req}};

        init_req_stream_rsc_dat <= {init_req, {`RID_WIDTH{1'b0}}};

        forever @(posedge clk)
            if (init_req_stream_rsc_rdy)
                break;
    end
end
// verilator lint_on INITIALDLY

initial begin
    int trv_req_fd;
    int num_correct;

    trv_req_fd = $fopen("trv_req.bin", "rb");
    assert(trv_req_fd != 0);

    num_correct = 0;
    for (int i = 0; i < `N_TESTCASES; i++) begin
        bit [14*32-1:0] trv_req;

        forever @(posedge clk)
            if (trv_req_stream_rsc_vld)
                break;
        
        if ($fread(trv_req, trv_req_fd) == 0)
            break;
        trv_req = {<<byte{trv_req}};

        if (trv_req_stream_rsc_dat === {trv_req, {`RID_WIDTH{1'b0}}}) begin
            num_correct++;
        end else begin
            $display("differ in %d", i);
        end

        if (i % 1000 == 0)
            $display("%d @ %t", i, $time);
    end

    $display("num_correct: %d", num_correct);
    $finish;
end

endmodule
