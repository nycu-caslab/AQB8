`timescale 1ns / 1ps
`include "../include/datatypes.svh"

`define CYCLE_TIME 2.0
`define N_TESTCASES 100000

module tb_clstr;

// input
logic                         clk;
logic                         arst_n;
logic [ `CLSTR_REQ_WIDTH-1:0] clstr_req_stream_rsc_dat;
logic                         clstr_req_stream_rsc_vld;
logic                         clstr_resp_stream_rsc_rdy;

// output
logic                         clstr_req_stream_rsc_rdy;
logic [`CLSTR_RESP_WIDTH-1:0] clstr_resp_stream_rsc_dat;
logic                         clstr_resp_stream_rsc_vld;

clstr DUT (
    .clk(clk),
    .arst_n(arst_n),
    .clstr_req_stream_rsc_dat(clstr_req_stream_rsc_dat),
    .clstr_req_stream_rsc_vld(clstr_req_stream_rsc_vld),
    .clstr_req_stream_rsc_rdy(clstr_req_stream_rsc_rdy),
    .clstr_resp_stream_rsc_dat(clstr_resp_stream_rsc_dat),
    .clstr_resp_stream_rsc_vld(clstr_resp_stream_rsc_vld),
    .clstr_resp_stream_rsc_rdy(clstr_resp_stream_rsc_rdy)
);

//////////////////////////////////////////////////

initial begin
    clk                       = 0;
    clstr_req_stream_rsc_dat  = 0;
    clstr_req_stream_rsc_vld  = 1;
    clstr_resp_stream_rsc_rdy = 1;

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
    int clstr_req_fd;

    clstr_req_fd = $fopen("clstr_req.bin", "rb");
    assert(clstr_req_fd != 0);

    forever begin
        bit [15*32-1:0] clstr_req;

        if ($fread(clstr_req, clstr_req_fd) == 0)
            break;
        clstr_req = {<<byte{clstr_req}};

        clstr_req_stream_rsc_dat <= {clstr_req, {`RID_WIDTH{1'b0}}};

        forever @(posedge clk)
            if (clstr_req_stream_rsc_rdy)
                break;
    end
end
// verilator lint_on INITIALDLY

initial begin
    int clstr_resp_fd;
    int num_correct;

    clstr_resp_fd = $fopen("clstr_resp.bin", "rb");
    assert(clstr_resp_fd != 0);

    num_correct = 0;
    for (int i = 0; i < `N_TESTCASES; i++) begin
        bit [8+6*32-1:0] clstr_resp;
        bit              correct;

        forever @(posedge clk)
            if (clstr_resp_stream_rsc_vld)
                break;
        
        if ($fread(clstr_resp, clstr_resp_fd) == 0)
            break;
        clstr_resp = {<<byte{clstr_resp}};

        correct = 0;
        if (clstr_resp[0] === 1) begin
            if (clstr_resp_stream_rsc_dat[0+:`RID_WIDTH] === {`RID_WIDTH{1'b0}} &&
                clstr_resp_stream_rsc_dat[`RID_WIDTH+0] === 1 &&
                clstr_resp_stream_rsc_dat[`RID_WIDTH+1+:6*32] === clstr_resp[8+:6*32])
                correct = 1;
        end else if (clstr_resp[0] === 0) begin
            if (clstr_resp_stream_rsc_dat[0+:`RID_WIDTH] === {`RID_WIDTH{1'b0}} &&
                clstr_resp_stream_rsc_dat[`RID_WIDTH+0] === 0)
                correct = 1;
        end else begin
            $fatal(1, "FATAL: INVALID clstr_resp");
        end

        if (correct) begin
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
