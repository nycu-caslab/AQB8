`timescale 1ns / 1ps
`include "../include/datatypes.svh"

`define CYCLE_TIME 2.0
`define N_TESTCASES 100000

module tb_ist;

// input
logic                       clk;
logic                       arst_n;
logic [ `IST_REQ_WIDTH-1:0] ist_req_stream_rsc_dat;
logic                       ist_req_stream_rsc_vld;
logic                       ist_resp_stream_rsc_rdy;

// output
logic                       ist_req_stream_rsc_rdy;
logic [`IST_RESP_WIDTH-1:0] ist_resp_stream_rsc_dat;
logic                       ist_resp_stream_rsc_vld;

ist DUT (
    .clk(clk),
    .arst_n(arst_n),
    .ist_req_stream_rsc_dat(ist_req_stream_rsc_dat),
    .ist_req_stream_rsc_vld(ist_req_stream_rsc_vld),
    .ist_req_stream_rsc_rdy(ist_req_stream_rsc_rdy),
    .ist_resp_stream_rsc_dat(ist_resp_stream_rsc_dat),
    .ist_resp_stream_rsc_vld(ist_resp_stream_rsc_vld),
    .ist_resp_stream_rsc_rdy(ist_resp_stream_rsc_rdy)
);

//////////////////////////////////////////////////

initial begin
    clk                     = 0;
    ist_req_stream_rsc_dat  = 0;
    ist_req_stream_rsc_vld  = 1;
    ist_resp_stream_rsc_rdy = 1;

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
    int ist_req_fd;

    ist_req_fd = $fopen("ist_req.bin", "rb");
    assert(ist_req_fd != 0);

    forever begin
        bit [17*32-1:0] ist_req;

        if ($fread(ist_req, ist_req_fd) == 0)
            break;
        ist_req = {<<byte{ist_req}};

        ist_req_stream_rsc_dat <= {ist_req, {`RID_WIDTH{1'b0}}};

        forever @(posedge clk)
            if (ist_req_stream_rsc_rdy)
                break;
    end
end
// verilator lint_on INITIALDLY

initial begin
    int ist_resp_fd;
    int num_correct;

    ist_resp_fd = $fopen("ist_resp.bin", "rb");
    assert(ist_resp_fd != 0);

    num_correct = 0;
    for (int i = 0; i < `N_TESTCASES; i++) begin
        bit [8+3*32-1:0] ist_resp;
        bit              correct;

        forever @(posedge clk)
            if (ist_resp_stream_rsc_vld)
                break;
        
        if ($fread(ist_resp, ist_resp_fd) == 0)
            break;
        ist_resp = {<<byte{ist_resp}};

        correct = 0;
        if (ist_resp[0] === 1) begin
            if (ist_resp_stream_rsc_dat[0+:`RID_WIDTH] === {`RID_WIDTH{1'b0}} &&
                ist_resp_stream_rsc_dat[`RID_WIDTH+0] === 1 &&
                ist_resp_stream_rsc_dat[`RID_WIDTH+1+32*0+:32] === ist_resp[8+32*0+:32] &&
                ist_resp_stream_rsc_dat[`RID_WIDTH+1+32*1+:32] === ist_resp[8+32*1+:32] &&
                ist_resp_stream_rsc_dat[`RID_WIDTH+1+32*2+:32] === ist_resp[8+32*2+:32])
                correct = 1;
        end else if (ist_resp[0] === 0) begin
            if (ist_resp_stream_rsc_dat[0+:`RID_WIDTH] === {`RID_WIDTH{1'b0}} &&
                ist_resp_stream_rsc_dat[`RID_WIDTH+0] === 0)
                correct = 1;
        end else begin
            $fatal(1, "FATAL: INVALID ist_resp");
        end

        if (correct) begin
            num_correct++;
        end else begin
            $display("differ in %d", i);
            $display("  %b (golden = %b)", ist_resp_stream_rsc_dat[`RID_WIDTH+0         ], ist_resp[         0]);
            $display("  %h (golden = %h)", ist_resp_stream_rsc_dat[`RID_WIDTH+1+32*0+:32], ist_resp[8+32*0+:32]);
            $display("  %h (golden = %h)", ist_resp_stream_rsc_dat[`RID_WIDTH+1+32*1+:32], ist_resp[8+32*1+:32]);
            $display("  %h (golden = %h)", ist_resp_stream_rsc_dat[`RID_WIDTH+1+32*2+:32], ist_resp[8+32*2+:32]);
        end

        if (i % 1000 == 0)
            $display("%d @ %t", i, $time);
    end

    $display("num_correct: %d", num_correct);
    $finish;
end

endmodule
