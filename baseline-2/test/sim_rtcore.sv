`timescale 1ns / 1ps
`include "../include/datatypes.svh"

module sim_rtcore (
    input                      clk,
    input                      arst_n,

    output                     ray_stream_full_n,
    input                      ray_stream_write,
    input  [   `RAY_WIDTH-1:0] ray_stream_din,

    output                     result_stream_empty_n,
    input                      result_stream_read,
    output [`RESULT_WIDTH-1:0] result_stream_dout
);

logic                              bbox_mem_req_stream_empty_n [0:`NUM_TRV-1];
logic                              bbox_mem_req_stream_read    [0:`NUM_TRV-1];
logic [   `BBOX_MEM_REQ_WIDTH-1:0] bbox_mem_req_stream_dout    [0:`NUM_TRV-1];

logic                              bbox_mem_resp_stream_full_n [0:`NUM_TRV-1];
logic                              bbox_mem_resp_stream_write  [0:`NUM_TRV-1];
logic [  `BBOX_MEM_RESP_WIDTH-1:0] bbox_mem_resp_stream_din    [0:`NUM_TRV-1];

logic                              ist_mem_req_stream_empty_n  [0:`NUM_TRV-1];
logic                              ist_mem_req_stream_read     [0:`NUM_TRV-1];
logic [    `IST_MEM_REQ_WIDTH-1:0] ist_mem_req_stream_dout     [0:`NUM_TRV-1];

logic                              ist_mem_resp_stream_full_n  [0:`NUM_TRV-1];
logic                              ist_mem_resp_stream_write   [0:`NUM_TRV-1];
logic [   `IST_MEM_RESP_WIDTH-1:0] ist_mem_resp_stream_din     [0:`NUM_TRV-1];

logic                              trig_sram_rd                [0:`NUM_TRV-1];
logic [ `TRIG_SRAM_ADDR_WIDTH-1:0] trig_sram_rd_addr           [0:`NUM_TRV-1];
logic [           `TRIG_WIDTH-1:0] trig_sram_rd_dout           [0:`NUM_TRV-1];
logic [           `TRIG_WIDTH-1:0] trig_sram                   [0:`NUM_TRV-1][0:`TRIG_SRAM_DEPTH-1];

logic                              node_stack_wr               [0:`NUM_TRV-1];
logic [`STACK_SRAM_ADDR_WIDTH-1:0] node_stack_wr_addr          [0:`NUM_TRV-1];
logic [           `NODE_WIDTH-1:0] node_stack_wr_din           [0:`NUM_TRV-1];
logic                              node_stack_rd               [0:`NUM_TRV-1];
logic [`STACK_SRAM_ADDR_WIDTH-1:0] node_stack_rd_addr          [0:`NUM_TRV-1];
logic [           `NODE_WIDTH-1:0] node_stack_rd_dout          [0:`NUM_TRV-1];

//////////////////////////////////////////////////

generate
    for (genvar gi = 0; gi < `NUM_TRV; gi++) begin : gen_mem
        sim_bbox_mem sim_bbox_mem_inst (
            .clk(clk),
            .arst_n(arst_n),

            .bbox_mem_req_stream_empty_n(bbox_mem_req_stream_empty_n[gi]),
            .bbox_mem_req_stream_read(bbox_mem_req_stream_read[gi]),
            .bbox_mem_req_stream_dout(bbox_mem_req_stream_dout[gi]),

            .bbox_mem_resp_stream_full_n(bbox_mem_resp_stream_full_n[gi]),
            .bbox_mem_resp_stream_write(bbox_mem_resp_stream_write[gi]),
            .bbox_mem_resp_stream_din(bbox_mem_resp_stream_din[gi])
        );

        sim_ist_mem sim_ist_mem_inst (
            .clk(clk),
            .arst_n(arst_n),

            .ist_mem_req_stream_empty_n(ist_mem_req_stream_empty_n[gi]),
            .ist_mem_req_stream_read(ist_mem_req_stream_read[gi]),
            .ist_mem_req_stream_dout(ist_mem_req_stream_dout[gi]),

            .ist_mem_resp_stream_full_n(ist_mem_resp_stream_full_n[gi]),
            .ist_mem_resp_stream_write(ist_mem_resp_stream_write[gi]),
            .ist_mem_resp_stream_din(ist_mem_resp_stream_din[gi]),

            .trig_sram(trig_sram[gi])
        );
    end
endgenerate

rtcore_top DUT (
    .clk(clk),
    .arst_n(arst_n),

    .ray_stream_full_n(ray_stream_full_n),
    .ray_stream_write(ray_stream_write),
    .ray_stream_din(ray_stream_din),

    .bbox_mem_resp_stream_full_n(bbox_mem_resp_stream_full_n),
    .bbox_mem_resp_stream_write(bbox_mem_resp_stream_write),
    .bbox_mem_resp_stream_din(bbox_mem_resp_stream_din),

    .ist_mem_resp_stream_full_n(ist_mem_resp_stream_full_n),
    .ist_mem_resp_stream_write(ist_mem_resp_stream_write),
    .ist_mem_resp_stream_din(ist_mem_resp_stream_din),

    .trig_sram_rd(trig_sram_rd),
    .trig_sram_rd_addr(trig_sram_rd_addr),
    .trig_sram_rd_dout(trig_sram_rd_dout),

    .node_stack_wr(node_stack_wr),
    .node_stack_wr_addr(node_stack_wr_addr),
    .node_stack_wr_din(node_stack_wr_din),
    .node_stack_rd(node_stack_rd),
    .node_stack_rd_addr(node_stack_rd_addr),
    .node_stack_rd_dout(node_stack_rd_dout),

    .bbox_mem_req_stream_empty_n(bbox_mem_req_stream_empty_n),
    .bbox_mem_req_stream_read(bbox_mem_req_stream_read),
    .bbox_mem_req_stream_dout(bbox_mem_req_stream_dout),

    .ist_mem_req_stream_empty_n(ist_mem_req_stream_empty_n),
    .ist_mem_req_stream_read(ist_mem_req_stream_read),
    .ist_mem_req_stream_dout(ist_mem_req_stream_dout),

    .result_stream_empty_n(result_stream_empty_n),
    .result_stream_read(result_stream_read),
    .result_stream_dout(result_stream_dout)
);

//////////////////////////////////////////////////

generate
    for (genvar gi = 0; gi < `NUM_TRV; gi++) begin : gen_sram
        always @(posedge clk) begin
            if (trig_sram_rd[gi])
                trig_sram_rd_dout[gi] <= trig_sram[gi][trig_sram_rd_addr[gi]];
        end

        sram_1r1w #(
            .DATA_WIDTH(`NODE_WIDTH),
            .DEPTH(`STACK_SRAM_DEPTH)
        ) node_stack_sram (
            .clk(clk),
            .arst_n(arst_n),
            .wr(node_stack_wr[gi]),
            .wr_addr(node_stack_wr_addr[gi]),
            .wr_din(node_stack_wr_din[gi]),
            .rd(node_stack_rd[gi]),
            .rd_addr(node_stack_rd_addr[gi]),
            .rd_dout(node_stack_rd_dout[gi])
        );
    end
endgenerate

endmodule
