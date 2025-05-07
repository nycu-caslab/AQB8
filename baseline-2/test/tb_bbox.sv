`timescale 1ns / 1ps
`include "../include/datatypes.svh"

`define CYCLE_TIME 2.0
`define N_TESTCASES 100000

module tb_bbox;

// input
logic                        clk;
logic                        arst_n;
logic [ `BBOX_REQ_WIDTH-1:0] bbox_req_stream_rsc_dat;
logic                        bbox_req_stream_rsc_vld;
logic                        bbox_resp_stream_rsc_rdy;

// output
logic                        bbox_req_stream_rsc_rdy;
logic [`BBOX_RESP_WIDTH-1:0] bbox_resp_stream_rsc_dat;
logic                        bbox_resp_stream_rsc_vld;

bbox DUT (
    .clk(clk),
    .arst_n(arst_n),
    .bbox_req_stream_rsc_dat(bbox_req_stream_rsc_dat),
    .bbox_req_stream_rsc_vld(bbox_req_stream_rsc_vld),
    .bbox_req_stream_rsc_rdy(bbox_req_stream_rsc_rdy),
    .bbox_resp_stream_rsc_dat(bbox_resp_stream_rsc_dat),
    .bbox_resp_stream_rsc_vld(bbox_resp_stream_rsc_vld),
    .bbox_resp_stream_rsc_rdy(bbox_resp_stream_rsc_rdy)
);

//////////////////////////////////////////////////

initial begin
    clk                      = 0;
    bbox_req_stream_rsc_dat  = 0;
    bbox_req_stream_rsc_vld  = 1;
    bbox_resp_stream_rsc_rdy = 1;

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
    int bbox_req_fd;

    bbox_req_fd = $fopen("bbox_req.bin", "rb");
    assert(bbox_req_fd != 0);

    forever begin
        bit [20*32-1:0] bbox_req;

        if ($fread(bbox_req, bbox_req_fd) == 0)
            break;
        bbox_req = {<<byte{bbox_req}};

        bbox_req_stream_rsc_dat <= {bbox_req, {`RID_WIDTH{1'b0}}};

        forever @(posedge clk)
            if (bbox_req_stream_rsc_rdy)
                break;
    end
end
// verilator lint_on INITIALDLY

initial begin
    int bbox_resp_fd;
    int num_correct;

    bbox_resp_fd = $fopen("bbox_resp.bin", "rb");
    assert(bbox_resp_fd != 0);

    num_correct = 0;
    for (int i = 0; i < `N_TESTCASES; i++) begin
        bit [3*8-1:0] bbox_resp;

        forever @(posedge clk)
            if (bbox_resp_stream_rsc_vld)
                break;
        
        if ($fread(bbox_resp, bbox_resp_fd) == 0)
            break;
        bbox_resp = {<<byte{bbox_resp}};

        if (bbox_resp_stream_rsc_dat[0+:`RID_WIDTH] === {`RID_WIDTH{1'b0}} &&
            bbox_resp_stream_rsc_dat[`RID_WIDTH+0] === bbox_resp[8*0] &&
            bbox_resp_stream_rsc_dat[`RID_WIDTH+1] === bbox_resp[8*1] &&
            bbox_resp_stream_rsc_dat[`RID_WIDTH+2] === bbox_resp[8*2]) begin
            num_correct++;
        end else begin
            $display("differ in %d", i);
            $display("  %b (golden = %b)", bbox_resp_stream_rsc_dat[`RID_WIDTH+0], bbox_resp[8*0]);
            $display("  %b (golden = %b)", bbox_resp_stream_rsc_dat[`RID_WIDTH+1], bbox_resp[8*1]);
            $display("  %b (golden = %b)", bbox_resp_stream_rsc_dat[`RID_WIDTH+2], bbox_resp[8*2]);
        end

        if (i % 1000 == 0)
            $display("%d @ %t", i, $time);
    end

    $display("num_correct: %d", num_correct);
    $finish;
end

endmodule
