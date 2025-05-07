`timescale 1ns / 1ps
`include "../include/datatypes.svh"

module rtcore_top (
    input                               clk,
    input                               arst_n,

    output                              ray_stream_full_n,
    input                               ray_stream_write,
    input  [            `RAY_WIDTH-1:0] ray_stream_din,

    output                              clstr_mem_resp_stream_full_n [0:`NUM_TRV-1],
    input                               clstr_mem_resp_stream_write  [0:`NUM_TRV-1],
    input  [ `CLSTR_MEM_RESP_WIDTH-1:0] clstr_mem_resp_stream_din    [0:`NUM_TRV-1],

    output                              bbox_mem_resp_stream_full_n  [0:`NUM_TRV-1],
    input                               bbox_mem_resp_stream_write   [0:`NUM_TRV-1],
    input  [  `BBOX_MEM_RESP_WIDTH-1:0] bbox_mem_resp_stream_din     [0:`NUM_TRV-1],

    output                              ist_mem_resp_stream_full_n   [0:`NUM_TRV-1],
    input                               ist_mem_resp_stream_write    [0:`NUM_TRV-1],
    input  [   `IST_MEM_RESP_WIDTH-1:0] ist_mem_resp_stream_din      [0:`NUM_TRV-1],

    output                              trig_sram_rd                 [0:`NUM_TRV-1],
    output [ `TRIG_SRAM_ADDR_WIDTH-1:0] trig_sram_rd_addr            [0:`NUM_TRV-1],
    input  [           `TRIG_WIDTH-1:0] trig_sram_rd_dout            [0:`NUM_TRV-1],

    output                              node_stack_wr                [0:`NUM_TRV-1],
    output [`STACK_SRAM_ADDR_WIDTH-1:0] node_stack_wr_addr           [0:`NUM_TRV-1],
    output [`NODE_STACK_DATA_WIDTH-1:0] node_stack_wr_din            [0:`NUM_TRV-1],
    output                              node_stack_rd                [0:`NUM_TRV-1],
    output [`STACK_SRAM_ADDR_WIDTH-1:0] node_stack_rd_addr           [0:`NUM_TRV-1],
    input  [`NODE_STACK_DATA_WIDTH-1:0] node_stack_rd_dout           [0:`NUM_TRV-1],

    output                              clstr_mem_req_stream_empty_n [0:`NUM_TRV-1],
    input                               clstr_mem_req_stream_read    [0:`NUM_TRV-1],
    output [  `CLSTR_MEM_REQ_WIDTH-1:0] clstr_mem_req_stream_dout    [0:`NUM_TRV-1],

    output                              bbox_mem_req_stream_empty_n  [0:`NUM_TRV-1],
    input                               bbox_mem_req_stream_read     [0:`NUM_TRV-1],
    output [   `BBOX_MEM_REQ_WIDTH-1:0] bbox_mem_req_stream_dout     [0:`NUM_TRV-1],

    output                              ist_mem_req_stream_empty_n   [0:`NUM_TRV-1],
    input                               ist_mem_req_stream_read      [0:`NUM_TRV-1],
    output [    `IST_MEM_REQ_WIDTH-1:0] ist_mem_req_stream_dout      [0:`NUM_TRV-1],

    output                              result_stream_empty_n,
    input                               result_stream_read,
    output [         `RESULT_WIDTH-1:0] result_stream_dout
);

wire                             init_req_stream_full_n       [0:  `NUM_TRV-1];
wire                             init_req_stream_write        [0:  `NUM_TRV-1];
wire [      `INIT_REQ_WIDTH-1:0] init_req_stream_din          [0:  `NUM_TRV-1];
wire                             init_req_stream_empty_n      [0: `NUM_INIT-1];
wire                             init_req_stream_read         [0: `NUM_INIT-1];
wire [      `INIT_REQ_WIDTH-1:0] init_req_stream_dout         [0: `NUM_INIT-1];

wire                             trv_resp_stream_full_n       [0:  `NUM_TRV-1];
wire                             trv_resp_stream_write        [0:  `NUM_TRV-1];
wire [      `TRV_RESP_WIDTH-1:0] trv_resp_stream_din          [0:  `NUM_TRV-1];
wire                             trv_resp_stream_empty_n      [0:  `NUM_TRV-1];
wire                             trv_resp_stream_read         [0:  `NUM_TRV-1];
wire [      `TRV_RESP_WIDTH-1:0] trv_resp_stream_dout         [0:  `NUM_TRV-1];

wire                             trv_req_stream_full_n        [0: `NUM_INIT-1];
wire                             trv_req_stream_write         [0: `NUM_INIT-1];
wire [       `TRV_REQ_WIDTH-1:0] trv_req_stream_din           [0: `NUM_INIT-1];
wire                             trv_req_stream_empty_n       [0:  `NUM_TRV-1];
wire                             trv_req_stream_read          [0:  `NUM_TRV-1];
wire [       `TRV_REQ_WIDTH-1:0] trv_req_stream_dout          [0:  `NUM_TRV-1];

wire                             clstr_mem_req_stream_full_n  [0:  `NUM_TRV-1];
wire                             clstr_mem_req_stream_write   [0:  `NUM_TRV-1];
wire [ `CLSTR_MEM_REQ_WIDTH-1:0] clstr_mem_req_stream_din     [0:  `NUM_TRV-1];

wire                             clstr_mem_resp_stream_empty_n[0:  `NUM_TRV-1];
wire                             clstr_mem_resp_stream_read   [0:  `NUM_TRV-1];
wire [`CLSTR_MEM_RESP_WIDTH-1:0] clstr_mem_resp_stream_dout   [0:  `NUM_TRV-1];

wire                             clstr_req_stream_full_n      [0:  `NUM_TRV-1];
wire                             clstr_req_stream_write       [0:  `NUM_TRV-1];
wire [     `CLSTR_REQ_WIDTH-1:0] clstr_req_stream_din         [0:  `NUM_TRV-1];
wire                             clstr_req_stream_empty_n     [0:`NUM_CLSTR-1];
wire                             clstr_req_stream_read        [0:`NUM_CLSTR-1];
wire [     `CLSTR_REQ_WIDTH-1:0] clstr_req_stream_dout        [0:`NUM_CLSTR-1];

wire                             clstr_resp_stream_full_n     [0:`NUM_CLSTR-1];
wire                             clstr_resp_stream_write      [0:`NUM_CLSTR-1];
wire [    `CLSTR_RESP_WIDTH-1:0] clstr_resp_stream_din        [0:`NUM_CLSTR-1];
wire                             clstr_resp_stream_empty_n    [0:  `NUM_TRV-1];
wire                             clstr_resp_stream_read       [0:  `NUM_TRV-1];
wire [    `CLSTR_RESP_WIDTH-1:0] clstr_resp_stream_dout       [0:  `NUM_TRV-1];

wire                             updt_req_stream_full_n       [0:  `NUM_TRV-1];
wire                             updt_req_stream_write        [0:  `NUM_TRV-1];
wire [      `UPDT_REQ_WIDTH-1:0] updt_req_stream_din          [0:  `NUM_TRV-1];
wire                             updt_req_stream_empty_n      [0: `NUM_UPDT-1];
wire                             updt_req_stream_read         [0: `NUM_UPDT-1];
wire [      `UPDT_REQ_WIDTH-1:0] updt_req_stream_dout         [0: `NUM_UPDT-1];

wire                             updt_resp_stream_full_n      [0: `NUM_UPDT-1];
wire                             updt_resp_stream_write       [0: `NUM_UPDT-1];
wire [     `UPDT_RESP_WIDTH-1:0] updt_resp_stream_din         [0: `NUM_UPDT-1];
wire                             updt_resp_stream_empty_n     [0:  `NUM_TRV-1];
wire                             updt_resp_stream_read        [0:  `NUM_TRV-1];
wire [     `UPDT_RESP_WIDTH-1:0] updt_resp_stream_dout        [0:  `NUM_TRV-1];

wire                             bbox_mem_req_stream_full_n   [0:  `NUM_TRV-1];
wire                             bbox_mem_req_stream_write    [0:  `NUM_TRV-1];
wire [  `BBOX_MEM_REQ_WIDTH-1:0] bbox_mem_req_stream_din      [0:  `NUM_TRV-1];

wire                             bbox_mem_resp_stream_empty_n [0:  `NUM_TRV-1];
wire                             bbox_mem_resp_stream_read    [0:  `NUM_TRV-1];
wire [ `BBOX_MEM_RESP_WIDTH-1:0] bbox_mem_resp_stream_dout    [0:  `NUM_TRV-1];

wire                             bbox_req_stream_full_n       [0:  `NUM_TRV-1];
wire                             bbox_req_stream_write        [0:  `NUM_TRV-1];
wire [      `BBOX_REQ_WIDTH-1:0] bbox_req_stream_din          [0:  `NUM_TRV-1];
wire                             bbox_req_stream_empty_n      [0: `NUM_BBOX-1];
wire                             bbox_req_stream_read         [0: `NUM_BBOX-1];
wire [      `BBOX_REQ_WIDTH-1:0] bbox_req_stream_dout         [0: `NUM_BBOX-1];

wire                             bbox_resp_stream_full_n      [0: `NUM_BBOX-1];
wire                             bbox_resp_stream_write       [0: `NUM_BBOX-1];
wire [     `BBOX_RESP_WIDTH-1:0] bbox_resp_stream_din         [0: `NUM_BBOX-1];
wire                             bbox_resp_stream_empty_n     [0:  `NUM_TRV-1];
wire                             bbox_resp_stream_read        [0:  `NUM_TRV-1];
wire [     `BBOX_RESP_WIDTH-1:0] bbox_resp_stream_dout        [0:  `NUM_TRV-1];

wire                             ist_mem_req_stream_full_n    [0:  `NUM_TRV-1];
wire                             ist_mem_req_stream_write     [0:  `NUM_TRV-1];
wire [   `IST_MEM_REQ_WIDTH-1:0] ist_mem_req_stream_din       [0:  `NUM_TRV-1];

wire                             ist_mem_resp_stream_empty_n  [0:  `NUM_TRV-1];
wire                             ist_mem_resp_stream_read     [0:  `NUM_TRV-1];
wire [  `IST_MEM_RESP_WIDTH-1:0] ist_mem_resp_stream_dout     [0:  `NUM_TRV-1];

wire                             ist_req_stream_full_n        [0:  `NUM_TRV-1];
wire                             ist_req_stream_write         [0:  `NUM_TRV-1];
wire [       `IST_REQ_WIDTH-1:0] ist_req_stream_din           [0:  `NUM_TRV-1];
wire                             ist_req_stream_empty_n       [0:  `NUM_IST-1];
wire                             ist_req_stream_read          [0:  `NUM_IST-1];
wire [       `IST_REQ_WIDTH-1:0] ist_req_stream_dout          [0:  `NUM_IST-1];

wire                             ist_resp_stream_full_n       [0:  `NUM_IST-1];
wire                             ist_resp_stream_write        [0:  `NUM_IST-1];
wire [      `IST_RESP_WIDTH-1:0] ist_resp_stream_din          [0:  `NUM_IST-1];
wire                             ist_resp_stream_empty_n      [0:  `NUM_TRV-1];
wire                             ist_resp_stream_read         [0:  `NUM_TRV-1];
wire [      `IST_RESP_WIDTH-1:0] ist_resp_stream_dout         [0:  `NUM_TRV-1];

//////////////////////////////////////////////////

logic                     internal_ray_stream_full_n     [0:`NUM_TRV-1];
logic                     internal_ray_stream_write      [0:`NUM_TRV-1];
logic [   `RAY_WIDTH-1:0] internal_ray_stream_din        [0:`NUM_TRV-1];
logic                     internal_ray_stream_empty_n    [0:`NUM_TRV-1];
logic                     internal_ray_stream_read       [0:`NUM_TRV-1];
logic [   `RAY_WIDTH-1:0] internal_ray_stream_dout       [0:`NUM_TRV-1];

logic                     internal_result_stream_full_n  [0:`NUM_TRV-1];
logic                     internal_result_stream_write   [0:`NUM_TRV-1];
logic [`RESULT_WIDTH-1:0] internal_result_stream_din     [0:`NUM_TRV-1];
logic                     internal_result_stream_empty_n [0:`NUM_TRV-1];
logic                     internal_result_stream_read    [0:`NUM_TRV-1];
logic [`RESULT_WIDTH-1:0] internal_result_stream_dout    [0:`NUM_TRV-1];

integer ray_curr;
integer result_curr;
                                                   
//////////////////////////////////////////////////

fifo_fw #(
    .M(`NUM_TRV),
    .N(`NUM_INIT),
    .DATA_WIDTH(`INIT_REQ_WIDTH)
) fifo_init_req (
    .clk(clk),
    .arst_n(arst_n),

    .full_n(init_req_stream_full_n),
    .write(init_req_stream_write),
    .din(init_req_stream_din),
    .empty_n(init_req_stream_empty_n),
    .read(init_req_stream_read),
    .dout(init_req_stream_dout)
);

fifo_multi #(
    .M(`NUM_TRV),
    .DATA_WIDTH(`TRV_RESP_WIDTH)
) fifo_trv_resp (
    .clk(clk),
    .arst_n(arst_n),

    .full_n(trv_resp_stream_full_n),
    .write(trv_resp_stream_write),
    .din(trv_resp_stream_din),
    .empty_n(trv_resp_stream_empty_n),
    .read(trv_resp_stream_read),
    .dout(trv_resp_stream_dout)
);

fifo_bw #(
    .M(`NUM_INIT),
    .N(`NUM_TRV),
    .DATA_WIDTH(`TRV_REQ_WIDTH)
) fifo_trv_req (
    .clk(clk),
    .arst_n(arst_n),

    .full_n(trv_req_stream_full_n),
    .write(trv_req_stream_write),
    .din(trv_req_stream_din),
    .empty_n(trv_req_stream_empty_n),
    .read(trv_req_stream_read),
    .dout(trv_req_stream_dout)
);

fifo_multi #(
    .M(`NUM_TRV),
    .DATA_WIDTH(`CLSTR_MEM_REQ_WIDTH)
) fifo_clstr_mem_req (
    .clk(clk),
    .arst_n(arst_n),

    .full_n(clstr_mem_req_stream_full_n),
    .write(clstr_mem_req_stream_write),
    .din(clstr_mem_req_stream_din),
    .empty_n(clstr_mem_req_stream_empty_n),
    .read(clstr_mem_req_stream_read),
    .dout(clstr_mem_req_stream_dout)
);

fifo_multi #(
    .M(`NUM_TRV),
    .DATA_WIDTH(`CLSTR_MEM_RESP_WIDTH)
) fifo_clstr_mem_resp (
    .clk(clk),
    .arst_n(arst_n),

    .full_n(clstr_mem_resp_stream_full_n),
    .write(clstr_mem_resp_stream_write),
    .din(clstr_mem_resp_stream_din),
    .empty_n(clstr_mem_resp_stream_empty_n),
    .read(clstr_mem_resp_stream_read),
    .dout(clstr_mem_resp_stream_dout)
);

fifo_fw #(
    .M(`NUM_TRV),
    .N(`NUM_CLSTR),
    .DATA_WIDTH(`CLSTR_REQ_WIDTH)
) fifo_clstr_req (
    .clk(clk),
    .arst_n(arst_n),

    .full_n(clstr_req_stream_full_n),
    .write(clstr_req_stream_write),
    .din(clstr_req_stream_din),
    .empty_n(clstr_req_stream_empty_n),
    .read(clstr_req_stream_read),
    .dout(clstr_req_stream_dout)
);

fifo_bw #(
    .M(`NUM_CLSTR),
    .N(`NUM_TRV),
    .DATA_WIDTH(`CLSTR_RESP_WIDTH)
) fifo_clstr_resp (
    .clk(clk),
    .arst_n(arst_n),

    .full_n(clstr_resp_stream_full_n),
    .write(clstr_resp_stream_write),
    .din(clstr_resp_stream_din),
    .empty_n(clstr_resp_stream_empty_n),
    .read(clstr_resp_stream_read),
    .dout(clstr_resp_stream_dout)
);

fifo_fw #(
    .M(`NUM_TRV),
    .N(`NUM_UPDT),
    .DATA_WIDTH(`UPDT_REQ_WIDTH)
) fifo_updt_req (
    .clk(clk),
    .arst_n(arst_n),

    .full_n(updt_req_stream_full_n),
    .write(updt_req_stream_write),
    .din(updt_req_stream_din),
    .empty_n(updt_req_stream_empty_n),
    .read(updt_req_stream_read),
    .dout(updt_req_stream_dout)
);

fifo_bw #(
    .M(`NUM_UPDT),
    .N(`NUM_TRV),
    .DATA_WIDTH(`UPDT_RESP_WIDTH)
) fifo_updt_resp (
    .clk(clk),
    .arst_n(arst_n),

    .full_n(updt_resp_stream_full_n),
    .write(updt_resp_stream_write),
    .din(updt_resp_stream_din),
    .empty_n(updt_resp_stream_empty_n),
    .read(updt_resp_stream_read),
    .dout(updt_resp_stream_dout)
);

fifo_multi #(
    .M(`NUM_TRV),
    .DATA_WIDTH(`BBOX_MEM_REQ_WIDTH)
) fifo_bbox_mem_req (
    .clk(clk),
    .arst_n(arst_n),

    .full_n(bbox_mem_req_stream_full_n),
    .write(bbox_mem_req_stream_write),
    .din(bbox_mem_req_stream_din),
    .empty_n(bbox_mem_req_stream_empty_n),
    .read(bbox_mem_req_stream_read),
    .dout(bbox_mem_req_stream_dout)
);

fifo_multi #(
    .M(`NUM_TRV),
    .DATA_WIDTH(`BBOX_MEM_RESP_WIDTH)
) fifo_bbox_mem_resp (
    .clk(clk),
    .arst_n(arst_n),

    .full_n(bbox_mem_resp_stream_full_n),
    .write(bbox_mem_resp_stream_write),
    .din(bbox_mem_resp_stream_din),
    .empty_n(bbox_mem_resp_stream_empty_n),
    .read(bbox_mem_resp_stream_read),
    .dout(bbox_mem_resp_stream_dout)
);

fifo_fw #(
    .M(`NUM_TRV),
    .N(`NUM_BBOX),
    .DATA_WIDTH(`BBOX_REQ_WIDTH)
) fifo_bbox_req (
    .clk(clk),
    .arst_n(arst_n),

    .full_n(bbox_req_stream_full_n),
    .write(bbox_req_stream_write),
    .din(bbox_req_stream_din),
    .empty_n(bbox_req_stream_empty_n),
    .read(bbox_req_stream_read),
    .dout(bbox_req_stream_dout)
);

fifo_bw #(
    .M(`NUM_BBOX),
    .N(`NUM_TRV),
    .DATA_WIDTH(`BBOX_RESP_WIDTH)
) fifo_bbox_resp (
    .clk(clk),
    .arst_n(arst_n),

    .full_n(bbox_resp_stream_full_n),
    .write(bbox_resp_stream_write),
    .din(bbox_resp_stream_din),
    .empty_n(bbox_resp_stream_empty_n),
    .read(bbox_resp_stream_read),
    .dout(bbox_resp_stream_dout)
);

fifo_multi #(
    .M(`NUM_TRV),
    .DATA_WIDTH(`IST_MEM_REQ_WIDTH)
) fifo_ist_mem_req (
    .clk(clk),
    .arst_n(arst_n),

    .full_n(ist_mem_req_stream_full_n),
    .write(ist_mem_req_stream_write),
    .din(ist_mem_req_stream_din),
    .empty_n(ist_mem_req_stream_empty_n),
    .read(ist_mem_req_stream_read),
    .dout(ist_mem_req_stream_dout)
);

fifo_multi #(
    .M(`NUM_TRV),
    .DATA_WIDTH(`IST_MEM_RESP_WIDTH)
) fifo_ist_mem_resp (
    .clk(clk),
    .arst_n(arst_n),

    .full_n(ist_mem_resp_stream_full_n),
    .write(ist_mem_resp_stream_write),
    .din(ist_mem_resp_stream_din),
    .empty_n(ist_mem_resp_stream_empty_n),
    .read(ist_mem_resp_stream_read),
    .dout(ist_mem_resp_stream_dout)
);

fifo_fw #(
    .M(`NUM_TRV),
    .N(`NUM_IST),
    .DATA_WIDTH(`IST_REQ_WIDTH)
) fifo_ist_req (
    .clk(clk),
    .arst_n(arst_n),

    .full_n(ist_req_stream_full_n),
    .write(ist_req_stream_write),
    .din(ist_req_stream_din),
    .empty_n(ist_req_stream_empty_n),
    .read(ist_req_stream_read),
    .dout(ist_req_stream_dout)
);

fifo_bw #(
    .M(`NUM_IST),
    .N(`NUM_TRV),
    .DATA_WIDTH(`IST_RESP_WIDTH)
) fifo_ist_resp (
    .clk(clk),
    .arst_n(arst_n),

    .full_n(ist_resp_stream_full_n),
    .write(ist_resp_stream_write),
    .din(ist_resp_stream_din),
    .empty_n(ist_resp_stream_empty_n),
    .read(ist_resp_stream_read),
    .dout(ist_resp_stream_dout)
);

//////////////////////////////////////////////////

generate 
    for (genvar gi = 0; gi < `NUM_TRV; gi++) begin : gen_reorder
        reorder #(
            .TID(gi)
        ) reorder_inst (
            .clk(clk),
            .arst_n(arst_n),

            .ray_stream_empty_n(internal_ray_stream_empty_n[gi]),
            .ray_stream_read(internal_ray_stream_read[gi]),
            .ray_stream_dout(internal_ray_stream_dout[gi]),

            .trv_resp_stream_empty_n(trv_resp_stream_empty_n[gi]),
            .trv_resp_stream_read(trv_resp_stream_read[gi]),
            .trv_resp_stream_dout(trv_resp_stream_dout[gi]),

            .init_req_stream_full_n(init_req_stream_full_n[gi]),
            .init_req_stream_write(init_req_stream_write[gi]),
            .init_req_stream_din(init_req_stream_din[gi]),

            .result_stream_full_n(internal_result_stream_full_n[gi]),
            .result_stream_write(internal_result_stream_write[gi]),
            .result_stream_din(internal_result_stream_din[gi])
        );
    end
endgenerate

generate 
    for (genvar gi = 0; gi < `NUM_TRV; gi++) begin : gen_trv
        trv trv_inst (
            .clk(clk),
            .arst_n(arst_n),

            .trv_req_stream_rsc_dat(trv_req_stream_dout[gi]), 
            .trv_req_stream_rsc_vld(trv_req_stream_empty_n[gi]), 
            .trv_req_stream_rsc_rdy(trv_req_stream_read[gi]),

            .clstr_mem_resp_stream_rsc_dat(clstr_mem_resp_stream_dout[gi]),
            .clstr_mem_resp_stream_rsc_vld(clstr_mem_resp_stream_empty_n[gi]),
            .clstr_mem_resp_stream_rsc_rdy(clstr_mem_resp_stream_read[gi]),

            .clstr_resp_stream_rsc_dat(clstr_resp_stream_dout[gi]),
            .clstr_resp_stream_rsc_vld(clstr_resp_stream_empty_n[gi]),
            .clstr_resp_stream_rsc_rdy(clstr_resp_stream_read[gi]),

            .updt_resp_stream_rsc_dat(updt_resp_stream_dout[gi]),
            .updt_resp_stream_rsc_vld(updt_resp_stream_empty_n[gi]),
            .updt_resp_stream_rsc_rdy(updt_resp_stream_read[gi]),

            .bbox_mem_resp_stream_rsc_dat(bbox_mem_resp_stream_dout[gi]),
            .bbox_mem_resp_stream_rsc_vld(bbox_mem_resp_stream_empty_n[gi]),
            .bbox_mem_resp_stream_rsc_rdy(bbox_mem_resp_stream_read[gi]),

            .bbox_resp_stream_rsc_dat(bbox_resp_stream_dout[gi]),
            .bbox_resp_stream_rsc_vld(bbox_resp_stream_empty_n[gi]),
            .bbox_resp_stream_rsc_rdy(bbox_resp_stream_read[gi]),

            .ist_mem_resp_stream_rsc_dat(ist_mem_resp_stream_dout[gi]),
            .ist_mem_resp_stream_rsc_vld(ist_mem_resp_stream_empty_n[gi]),
            .ist_mem_resp_stream_rsc_rdy(ist_mem_resp_stream_read[gi]),

            .ist_resp_stream_rsc_dat(ist_resp_stream_dout[gi]),
            .ist_resp_stream_rsc_vld(ist_resp_stream_empty_n[gi]),
            .ist_resp_stream_rsc_rdy(ist_resp_stream_read[gi]),

            .trig_sram_rd(trig_sram_rd[gi]),
            .trig_sram_rd_addr(trig_sram_rd_addr[gi]),
            .trig_sram_rd_dout(trig_sram_rd_dout[gi]),

            .node_stack_wr(node_stack_wr[gi]),
            .node_stack_wr_addr(node_stack_wr_addr[gi]),
            .node_stack_wr_din(node_stack_wr_din[gi]),
            .node_stack_rd(node_stack_rd[gi]),
            .node_stack_rd_addr(node_stack_rd_addr[gi]),
            .node_stack_rd_dout(node_stack_rd_dout[gi]),

            .clstr_mem_req_stream_rsc_dat(clstr_mem_req_stream_din[gi]),
            .clstr_mem_req_stream_rsc_vld(clstr_mem_req_stream_write[gi]),
            .clstr_mem_req_stream_rsc_rdy(clstr_mem_req_stream_full_n[gi]),

            .clstr_req_stream_rsc_dat(clstr_req_stream_din[gi]),
            .clstr_req_stream_rsc_vld(clstr_req_stream_write[gi]),
            .clstr_req_stream_rsc_rdy(clstr_req_stream_full_n[gi]),

            .updt_req_stream_rsc_dat(updt_req_stream_din[gi]),
            .updt_req_stream_rsc_vld(updt_req_stream_write[gi]),
            .updt_req_stream_rsc_rdy(updt_req_stream_full_n[gi]),

            .bbox_mem_req_stream_rsc_dat(bbox_mem_req_stream_din[gi]),
            .bbox_mem_req_stream_rsc_vld(bbox_mem_req_stream_write[gi]),
            .bbox_mem_req_stream_rsc_rdy(bbox_mem_req_stream_full_n[gi]),

            .bbox_req_stream_rsc_dat(bbox_req_stream_din[gi]), 
            .bbox_req_stream_rsc_vld(bbox_req_stream_write[gi]),
            .bbox_req_stream_rsc_rdy(bbox_req_stream_full_n[gi]),

            .ist_mem_req_stream_rsc_dat(ist_mem_req_stream_din[gi]),
            .ist_mem_req_stream_rsc_vld(ist_mem_req_stream_write[gi]),
            .ist_mem_req_stream_rsc_rdy(ist_mem_req_stream_full_n[gi]),

            .ist_req_stream_rsc_dat(ist_req_stream_din[gi]),
            .ist_req_stream_rsc_vld(ist_req_stream_write[gi]),
            .ist_req_stream_rsc_rdy(ist_req_stream_full_n[gi]),
            
            .trv_resp_stream_rsc_dat(trv_resp_stream_din[gi]),
            .trv_resp_stream_rsc_vld(trv_resp_stream_write[gi]),
            .trv_resp_stream_rsc_rdy(trv_resp_stream_full_n[gi])
        );
    end
endgenerate

generate
    for (genvar gi = 0; gi < `NUM_INIT; gi++) begin : gen_init
        init init_inst (
            .clk(clk),
            .arst_n(arst_n),

            .init_req_stream_rsc_dat(init_req_stream_dout[gi]),
            .init_req_stream_rsc_vld(init_req_stream_empty_n[gi]),
            .init_req_stream_rsc_rdy(init_req_stream_read[gi]),

            .trv_req_stream_rsc_dat(trv_req_stream_din[gi]),
            .trv_req_stream_rsc_vld(trv_req_stream_write[gi]),
            .trv_req_stream_rsc_rdy(trv_req_stream_full_n[gi])
        );
    end
endgenerate

generate
    for (genvar gi = 0; gi < `NUM_CLSTR; gi++) begin : gen_clstr
        clstr clstr_inst (
            .clk(clk),
            .arst_n(arst_n),

            .clstr_req_stream_rsc_dat(clstr_req_stream_dout[gi]),
            .clstr_req_stream_rsc_vld(clstr_req_stream_empty_n[gi]),
            .clstr_req_stream_rsc_rdy(clstr_req_stream_read[gi]),

            .clstr_resp_stream_rsc_dat(clstr_resp_stream_din[gi]),
            .clstr_resp_stream_rsc_vld(clstr_resp_stream_write[gi]),
            .clstr_resp_stream_rsc_rdy(clstr_resp_stream_full_n[gi])
        );
    end
endgenerate

generate
    for (genvar gi = 0; gi < `NUM_UPDT; gi++) begin : gen_updt
        updt updt_inst (
            .clk(clk),
            .arst_n(arst_n),

            .updt_req_stream_rsc_dat(updt_req_stream_dout[gi]),
            .updt_req_stream_rsc_vld(updt_req_stream_empty_n[gi]),
            .updt_req_stream_rsc_rdy(updt_req_stream_read[gi]),

            .updt_resp_stream_rsc_dat(updt_resp_stream_din[gi]),
            .updt_resp_stream_rsc_vld(updt_resp_stream_write[gi]),
            .updt_resp_stream_rsc_rdy(updt_resp_stream_full_n[gi])
        );
    end
endgenerate

generate
    for (genvar gi = 0; gi < `NUM_BBOX; gi++) begin : gen_bbox
        bbox bbox_inst (
            .clk(clk),
            .arst_n(arst_n),

            .bbox_req_stream_rsc_dat(bbox_req_stream_dout[gi]),
            .bbox_req_stream_rsc_vld(bbox_req_stream_empty_n[gi]),
            .bbox_req_stream_rsc_rdy(bbox_req_stream_read[gi]),

            .bbox_resp_stream_rsc_dat(bbox_resp_stream_din[gi]),
            .bbox_resp_stream_rsc_vld(bbox_resp_stream_write[gi]),
            .bbox_resp_stream_rsc_rdy(bbox_resp_stream_full_n[gi])
        );
    end
endgenerate

generate
    for (genvar gi = 0; gi < `NUM_IST; gi++) begin : gen_ist
        ist ist_inst (
            .clk(clk),
            .arst_n(arst_n),

            .ist_req_stream_rsc_dat(ist_req_stream_dout[gi]),
            .ist_req_stream_rsc_vld(ist_req_stream_empty_n[gi]),
            .ist_req_stream_rsc_rdy(ist_req_stream_read[gi]),

            .ist_resp_stream_rsc_dat(ist_resp_stream_din[gi]),
            .ist_resp_stream_rsc_vld(ist_resp_stream_write[gi]),
            .ist_resp_stream_rsc_rdy(ist_resp_stream_full_n[gi])
        );
    end
endgenerate

//////////////////////////////////////////////////

assign ray_stream_full_n     = internal_ray_stream_full_n[ray_curr];
assign result_stream_empty_n = internal_result_stream_empty_n[result_curr];
assign result_stream_dout    = internal_result_stream_dout[result_curr];

fifo_multi #(
    .M(`NUM_TRV),
    .DATA_WIDTH(`RAY_WIDTH)
) fifo_internal_ray (
    .clk(clk),
    .arst_n(arst_n),

    .full_n(internal_ray_stream_full_n),
    .write(internal_ray_stream_write),
    .din(internal_ray_stream_din),
    .empty_n(internal_ray_stream_empty_n),
    .read(internal_ray_stream_read),
    .dout(internal_ray_stream_dout)
);

fifo_multi #(
    .M(`NUM_TRV),
    .DATA_WIDTH(`RESULT_WIDTH)
) fifo_internal_result (
    .clk(clk),
    .arst_n(arst_n),

    .full_n(internal_result_stream_full_n),
    .write(internal_result_stream_write),
    .din(internal_result_stream_din),
    .empty_n(internal_result_stream_empty_n),
    .read(internal_result_stream_read),
    .dout(internal_result_stream_dout)
);

generate
    for (genvar gi = 0; gi < `NUM_TRV; gi++) begin : gen_internal
        assign internal_ray_stream_write[gi] = (ray_curr == gi) && ray_stream_write;
        assign internal_ray_stream_din[gi] = ray_stream_din;
        assign internal_result_stream_read[gi] = (result_curr == gi) && result_stream_read;
    end
endgenerate

always @(posedge clk or negedge arst_n) begin
    if (~arst_n)
        ray_curr <= 0;
    else if (ray_stream_full_n && ray_stream_write)
        ray_curr <= (ray_curr == `NUM_TRV - 1) ? 0 : ray_curr + 1;
end

always @(posedge clk or negedge arst_n) begin
    if (~arst_n)
        result_curr <= 0;
    else if (result_stream_empty_n && result_stream_read)
        result_curr <= (result_curr == `NUM_TRV - 1) ? 0 : result_curr + 1;
end

endmodule
