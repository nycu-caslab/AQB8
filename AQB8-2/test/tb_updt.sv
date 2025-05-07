`timescale 1ns / 1ps
`include "../include/datatypes.svh"

`define CYCLE_TIME 2.0
`define N_TESTCASES 100000

`define UPDT_REQ_WIDTH  (`RID_WIDTH + 3*32)
`define UPDT_RESP_WIDTH (`RID_WIDTH + 32)

module tb_updt;

// input
logic                        clk;
logic                        arst_n;
logic [ `UPDT_REQ_WIDTH-1:0] updt_req_stream_rsc_dat;
logic                        updt_req_stream_rsc_vld;
logic                        updt_resp_stream_rsc_rdy;

// output
logic                        updt_req_stream_rsc_rdy;
logic [`UPDT_RESP_WIDTH-1:0] updt_resp_stream_rsc_dat;
logic                        updt_resp_stream_rsc_vld;

updt DUT (
    .clk(clk),
    .arst_n(arst_n),
    .updt_req_stream_rsc_dat(updt_req_stream_rsc_dat),
    .updt_req_stream_rsc_vld(updt_req_stream_rsc_vld),
    .updt_req_stream_rsc_rdy(updt_req_stream_rsc_rdy),
    .updt_resp_stream_rsc_dat(updt_resp_stream_rsc_dat),
    .updt_resp_stream_rsc_vld(updt_resp_stream_rsc_vld),
    .updt_resp_stream_rsc_rdy(updt_resp_stream_rsc_rdy)
);

//////////////////////////////////////////////////

initial begin
    clk                      = 0;
    updt_req_stream_rsc_dat  = 0;
    updt_req_stream_rsc_vld  = 1;
    updt_resp_stream_rsc_rdy = 1;

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
    int updt_req_fd;

    updt_req_fd = $fopen("updt_req.bin", "rb");
    assert(updt_req_fd != 0);

    forever begin
        bit [3*32-1:0] updt_req;

        if ($fread(updt_req, updt_req_fd) == 0)
            break;
        updt_req = {<<byte{updt_req}};

        updt_req_stream_rsc_dat <= {updt_req, {`RID_WIDTH{1'b0}}};

        forever @(posedge clk)
            if (updt_req_stream_rsc_rdy)
                break;
    end
end
// verilator lint_on INITIALDLY

initial begin
    int updt_resp_fd;
    int num_correct;

    updt_resp_fd = $fopen("updt_resp.bin", "rb");
    assert(updt_resp_fd != 0);

    num_correct = 0;
    for (int i = 0; i < `N_TESTCASES; i++) begin
        bit [31:0] updt_resp;

        forever @(posedge clk)
            if (updt_resp_stream_rsc_vld)
                break;
        
        if ($fread(updt_resp, updt_resp_fd) == 0)
            break;
        updt_resp = {<<byte{updt_resp}};

        if (updt_resp_stream_rsc_dat[`RID_WIDTH+:32] == updt_resp) begin
            num_correct++;
        end else begin
            $display("differ in %d", i);
            $display("  %h (golden = %h)", updt_resp_stream_rsc_dat[`RID_WIDTH+:32], updt_resp);
        end

        if (i % 1000 == 0)
            $display("%d @ %t", i, $time);
    end

    $display("num_correct: %d", num_correct);
    $finish;
end

endmodule
