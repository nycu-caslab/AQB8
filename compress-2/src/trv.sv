`timescale 1ns / 1ps
`include "../include/datatypes.svh"

`define MAX(a, b)        ((a) > (b) ? (a) : (b))

`define INPUT_REG_WIDTH  `MAX(`TRV_REQ_WIDTH, `MAX(`BBOX_MEM_RESP_WIDTH, `MAX(`BBOX_RESP_WIDTH, `MAX(`IST_MEM_RESP_WIDTH, `IST_RESP_WIDTH))))

`define TRV_REQ       1
`define BBOX_MEM_RESP 2
`define BBOX_RESP     3
`define IST_MEM_RESP  4
`define IST_RESP      5

`define BBOX_MEM_REQ  1
`define BBOX_REQ      2
`define IST_MEM_REQ   3
`define IST_REQ       4
`define TRV_RESP      5

// verilator lint_off DECLFILENAME
module reg_module #(
    parameter BITS = 1
) (
    input             clk,
    input             arst_n,
    input             WE,
    input  [BITS-1:0] D,
    output [BITS-1:0] Q
);

reg [BITS-1:0] data;

always_ff @(posedge clk or negedge arst_n) begin
    if (~arst_n)
        data <= {BITS{1'b0}};
    else if (WE)
        data <= D;
end

assign Q = data;

endmodule
// verilator lint_on DECLFILENAME

module trv (
    input                               clk,
    input                               arst_n,

    input  [        `TRV_REQ_WIDTH-1:0] trv_req_stream_rsc_dat, 
    input                               trv_req_stream_rsc_vld, 
    output                              trv_req_stream_rsc_rdy,

    input  [  `BBOX_MEM_RESP_WIDTH-1:0] bbox_mem_resp_stream_rsc_dat,
    input                               bbox_mem_resp_stream_rsc_vld,
    output                              bbox_mem_resp_stream_rsc_rdy,

    input  [      `BBOX_RESP_WIDTH-1:0] bbox_resp_stream_rsc_dat,
    input                               bbox_resp_stream_rsc_vld,
    output                              bbox_resp_stream_rsc_rdy,

    input  [   `IST_MEM_RESP_WIDTH-1:0] ist_mem_resp_stream_rsc_dat, 
    input                               ist_mem_resp_stream_rsc_vld,
    output                              ist_mem_resp_stream_rsc_rdy,

    input  [       `IST_RESP_WIDTH-1:0] ist_resp_stream_rsc_dat,
    input                               ist_resp_stream_rsc_vld,
    output                              ist_resp_stream_rsc_rdy,

    output                              trig_sram_rd,
    output [ `TRIG_SRAM_ADDR_WIDTH-1:0] trig_sram_rd_addr, 
    input  [           `TRIG_WIDTH-1:0] trig_sram_rd_dout,

    output                              node_stack_wr,
    output [`STACK_SRAM_ADDR_WIDTH-1:0] node_stack_wr_addr,
    output [`NODE_STACK_DATA_WIDTH-1:0] node_stack_wr_din,
    output                              node_stack_rd,
    output [`STACK_SRAM_ADDR_WIDTH-1:0] node_stack_rd_addr,
    input  [`NODE_STACK_DATA_WIDTH-1:0] node_stack_rd_dout,

    output [   `BBOX_MEM_REQ_WIDTH-1:0] bbox_mem_req_stream_rsc_dat,
    output                              bbox_mem_req_stream_rsc_vld,
    input                               bbox_mem_req_stream_rsc_rdy,

    output [       `BBOX_REQ_WIDTH-1:0] bbox_req_stream_rsc_dat, 
    output                              bbox_req_stream_rsc_vld,
    input                               bbox_req_stream_rsc_rdy,

    output [    `IST_MEM_REQ_WIDTH-1:0] ist_mem_req_stream_rsc_dat,
    output                              ist_mem_req_stream_rsc_vld,
    input                               ist_mem_req_stream_rsc_rdy,

    output [        `IST_REQ_WIDTH-1:0] ist_req_stream_rsc_dat,
    output                              ist_req_stream_rsc_vld,
    input                               ist_req_stream_rsc_rdy,
    
    output [       `TRV_RESP_WIDTH-1:0] trv_resp_stream_rsc_dat,
    output                              trv_resp_stream_rsc_vld,
    input                               trv_resp_stream_rsc_rdy
);

// (s1)             | (s2)       | (s3)       
// read transaction | read local | write local
// read local       | read trig  | write stack
//                  | read stack | write transaction 

// local_mem /////////////////////////////////////

logic                              local_mem_a_wr;
logic [    `LOCAL_MEM_A_WIDTH-1:0] local_mem_a_wr_din;
logic [    `LOCAL_MEM_A_WIDTH-1:0] local_mem_a_rd_dout;

logic                              local_mem_b_wr;
logic [    `LOCAL_MEM_B_WIDTH-1:0] local_mem_b_wr_din;
logic [    `LOCAL_MEM_B_WIDTH-1:0] local_mem_b_rd_dout;

logic                              local_mem_c_wr;
logic [    `LOCAL_MEM_C_WIDTH-1:0] local_mem_c_wr_din;
logic [    `LOCAL_MEM_C_WIDTH-1:0] local_mem_c_rd_dout;

logic                              local_mem_d_wr;
logic [    `LOCAL_MEM_D_WIDTH-1:0] local_mem_d_wr_din;
logic [    `LOCAL_MEM_D_WIDTH-1:0] local_mem_d_rd_dout;

logic                              local_mem_e_wr;
logic [    `LOCAL_MEM_E_WIDTH-1:0] local_mem_e_wr_din;
logic [    `LOCAL_MEM_E_WIDTH-1:0] local_mem_e_rd_dout;

logic                              local_mem_f_wr;
logic [    `LOCAL_MEM_F_WIDTH-1:0] local_mem_f_wr_din;
logic [    `LOCAL_MEM_F_WIDTH-1:0] local_mem_f_rd_dout;

logic                              local_mem_g_wr;
logic [    `LOCAL_MEM_G_WIDTH-1:0] local_mem_g_wr_din;
logic [    `LOCAL_MEM_G_WIDTH-1:0] local_mem_g_rd_dout;

logic                              local_mem_h_wr;
logic [    `LOCAL_MEM_H_WIDTH-1:0] local_mem_h_wr_din;
logic [    `LOCAL_MEM_H_WIDTH-1:0] local_mem_h_rd_dout;

logic                              local_mem_i_wr;
logic [    `LOCAL_MEM_I_WIDTH-1:0] local_mem_i_wr_din;
logic [    `LOCAL_MEM_I_WIDTH-1:0] local_mem_i_rd_dout;

reg_module #(`LOCAL_MEM_A_WIDTH) local_mem_a (
    .clk(clk),
    .arst_n(arst_n),
    .WE(local_mem_a_wr),
    .D(local_mem_a_wr_din),
    .Q(local_mem_a_rd_dout)
);

reg_module #(`LOCAL_MEM_B_WIDTH) local_mem_b (
    .clk(clk),
    .arst_n(arst_n),
    .WE(local_mem_b_wr),
    .D(local_mem_b_wr_din),
    .Q(local_mem_b_rd_dout)
);

reg_module #(`LOCAL_MEM_C_WIDTH) local_mem_c (
    .clk(clk),
    .arst_n(arst_n),
    .WE(local_mem_c_wr),
    .D(local_mem_c_wr_din),
    .Q(local_mem_c_rd_dout)
);

reg_module #(`LOCAL_MEM_D_WIDTH) local_mem_d (
    .clk(clk),
    .arst_n(arst_n),
    .WE(local_mem_d_wr),
    .D(local_mem_d_wr_din),
    .Q(local_mem_d_rd_dout)
);

reg_module #(`LOCAL_MEM_E_WIDTH) local_mem_e (
    .clk(clk),
    .arst_n(arst_n),
    .WE(local_mem_e_wr),
    .D(local_mem_e_wr_din),
    .Q(local_mem_e_rd_dout)
);

reg_module #(`LOCAL_MEM_F_WIDTH) local_mem_f (
    .clk(clk),
    .arst_n(arst_n),
    .WE(local_mem_f_wr),
    .D(local_mem_f_wr_din),
    .Q(local_mem_f_rd_dout)
);

reg_module #(`LOCAL_MEM_G_WIDTH) local_mem_g (
    .clk(clk),
    .arst_n(arst_n),
    .WE(local_mem_g_wr),
    .D(local_mem_g_wr_din),
    .Q(local_mem_g_rd_dout)
);

reg_module #(`LOCAL_MEM_H_WIDTH) local_mem_h (
    .clk(clk),
    .arst_n(arst_n),
    .WE(local_mem_h_wr),
    .D(local_mem_h_wr_din),
    .Q(local_mem_h_rd_dout)
);

reg_module #(`LOCAL_MEM_I_WIDTH) local_mem_i (
    .clk(clk),
    .arst_n(arst_n),
    .WE(local_mem_i_wr),
    .D(local_mem_i_wr_din),
    .Q(local_mem_i_rd_dout)
);

// global ////////////////////////////////////////

localparam S2 = 0,
           S3 = 1;
logic [0:0] state;
logic [0:0] next_state;

logic [                 2:0] input_type;
logic [`INPUT_REG_WIDTH-1:0] input_reg;

logic stall;

local_mem_a_t local_mem_a_nxt;
local_mem_b_t local_mem_b_nxt;
local_mem_c_t local_mem_c_nxt;
local_mem_d_t local_mem_d_nxt;
local_mem_e_t local_mem_e_nxt;
local_mem_f_t local_mem_f_nxt;
local_mem_g_t local_mem_g_nxt;
local_mem_h_t local_mem_h_nxt;
local_mem_i_t local_mem_i_nxt;

// stage 2 ///////////////////////////////////////

logic [                 2:0] s2_input_type;
logic [`INPUT_REG_WIDTH-1:0] s2_input_reg;

logic [`CID_WIDTH-1:0] s2_cid;

local_mem_a_t s2_local_mem_a;
local_mem_h_t s2_local_mem_h;

stack_size_t s2_local_mem_node_stack_size_decr;
num_trigs_t  s2_local_mem_num_trigs_left_decr;

// verilator lint_off UNUSEDSIGNAL
bbox_resp_t s2_bbox_resp;
// verilator lint_on UNUSEDSIGNAL

logic s2_send_ist_req;

logic s2_stack_op;
logic s2_send_trv_resp;
logic s2_pop_node_stack;

// stage 3 ///////////////////////////////////////

logic [                 2:0] s3_input_type;
logic [`INPUT_REG_WIDTH-1:0] s3_input_reg;

rid_t                  s3_rid;
logic [`CID_WIDTH-1:0] s3_cid;

local_mem_a_t s3_local_mem_a;
local_mem_b_t s3_local_mem_b;
local_mem_c_t s3_local_mem_c;
local_mem_d_t s3_local_mem_d;
local_mem_e_t s3_local_mem_e;
local_mem_f_t s3_local_mem_f;
local_mem_g_t s3_local_mem_g;
local_mem_h_t s3_local_mem_h;
local_mem_i_t s3_local_mem_i;

stack_size_t s3_local_mem_node_stack_size_incr;
stack_size_t s3_local_mem_node_stack_size_decr;
num_trigs_t  s3_local_mem_num_trigs_left_decr;

trv_req_t       s3_trv_req;
bbox_mem_resp_t s3_bbox_mem_resp;
bbox_resp_t     s3_bbox_resp;
ist_mem_resp_t  s3_ist_mem_resp;
ist_resp_t      s3_ist_resp;

// verilator lint_off UNUSEDSIGNAL
rid_t s3_trv_req_rid;
rid_t s3_bbox_mem_resp_rid;
rid_t s3_bbox_resp_rid;
rid_t s3_ist_mem_resp_rid;
rid_t s3_ist_resp_rid;
// verilator lint_on UNUSEDSIGNAL

logic s3_send_ist_req;

logic       s3_send_mem_req;
node_t      s3_send_mem_req_node;
logic       s3_send_mem_req_ist;
logic [2:0] s3_send_mem_req_output_type;

logic       s3_stack_op;
logic       s3_pop_node_stack;
node_t      s3_node_stack_top_node;
ref_bbox_t  s3_node_stack_top_b;
logic [2:0] s3_stack_op_output_type;

logic s3_push_node_stack;

logic  s3_left_is_leaf;
logic  s3_right_is_leaf;
logic  s3_left_first;
node_t s3_first_node;
node_t s3_second_node;
ref_bbox_t s3_first_b;
ref_bbox_t s3_second_b;

bbox_mem_req_t s3_bbox_mem_req;
bbox_req_t     s3_bbox_req;
ist_mem_req_t  s3_ist_mem_req;
ist_req_t      s3_ist_req;
trv_resp_t     s3_trv_resp;

logic [2:0] s3_output_type;

// io ////////////////////////////////////////////

assign trv_req_stream_rsc_rdy       = (~stall) && (s3_input_type == `TRV_REQ);
assign bbox_mem_resp_stream_rsc_rdy = (~stall) && (s3_input_type == `BBOX_MEM_RESP);
assign bbox_resp_stream_rsc_rdy     = (~stall) && (s3_input_type == `BBOX_RESP);
assign ist_mem_resp_stream_rsc_rdy  = (~stall) && (s3_input_type == `IST_MEM_RESP);
assign ist_resp_stream_rsc_rdy      = (~stall) && (s3_input_type == `IST_RESP);

assign trig_sram_rd      = (~stall) && s2_send_ist_req;
assign trig_sram_rd_addr = {s2_local_mem_num_trigs_left_decr, s2_cid};

assign node_stack_wr      = (~stall) && s3_push_node_stack;
assign node_stack_wr_addr = {s3_local_mem_a.node_stack_size, s3_cid};
assign node_stack_wr_din  = {s3_second_node, s3_second_b};
assign node_stack_rd      = (~stall) && s2_pop_node_stack;
assign node_stack_rd_addr = {s2_local_mem_node_stack_size_decr, s2_cid};

assign local_mem_a_wr      = (~stall) && (s3_pop_node_stack || s3_input_type == `TRV_REQ || s3_push_node_stack);
assign local_mem_a_wr_din  = local_mem_a_nxt;

assign local_mem_b_wr      = (~stall) && s3_input_type == `TRV_REQ;
assign local_mem_b_wr_din  = local_mem_b_nxt;

assign local_mem_c_wr      = local_mem_b_wr;
assign local_mem_c_wr_din  = local_mem_c_nxt;

assign local_mem_d_wr      = local_mem_b_wr;
assign local_mem_d_wr_din  = local_mem_d_nxt;

assign local_mem_e_wr      = (~stall) && (s3_input_type == `TRV_REQ || (s3_input_type == `IST_RESP && s3_ist_resp.intersected));
assign local_mem_e_wr_din  = local_mem_e_nxt;

assign local_mem_f_wr      = local_mem_e_wr;
assign local_mem_f_wr_din  = local_mem_f_nxt;

assign local_mem_g_wr      = (~stall) && (s3_input_type == `BBOX_MEM_RESP);
assign local_mem_g_wr_din  = local_mem_g_nxt;

assign local_mem_h_wr      = (~stall) && (s3_send_ist_req || s3_send_mem_req_ist);
assign local_mem_h_wr_din  = local_mem_h_nxt;

assign local_mem_i_wr      = (~stall) && (s3_input_type == `TRV_REQ || s3_send_mem_req);
assign local_mem_i_wr_din  = local_mem_i_nxt;

assign bbox_mem_req_stream_rsc_dat = {
    s3_bbox_mem_req.nbp_idx,
    s3_bbox_mem_req.rid
};
assign bbox_mem_req_stream_rsc_vld = (s3_output_type == `BBOX_MEM_REQ);

assign bbox_req_stream_rsc_dat = {
    s3_bbox_req.right_bbox.z_max,
    s3_bbox_req.right_bbox.z_min,
    s3_bbox_req.right_bbox.y_max,
    s3_bbox_req.right_bbox.y_min,
    s3_bbox_req.right_bbox.x_max,
    s3_bbox_req.right_bbox.x_min,
    s3_bbox_req.left_bbox.z_max,
    s3_bbox_req.left_bbox.z_min,
    s3_bbox_req.left_bbox.y_max,
    s3_bbox_req.left_bbox.y_min,
    s3_bbox_req.left_bbox.x_max,
    s3_bbox_req.left_bbox.x_min,
    s3_bbox_req.exp.z,
    s3_bbox_req.exp.y,
    s3_bbox_req.exp.x,
    s3_bbox_req.preprocessed_ray.tmax,
    s3_bbox_req.preprocessed_ray.tmin,
    s3_bbox_req.preprocessed_ray.b_z,
    s3_bbox_req.preprocessed_ray.b_y,
    s3_bbox_req.preprocessed_ray.b_x,
    s3_bbox_req.preprocessed_ray.w_z,
    s3_bbox_req.preprocessed_ray.w_y,
    s3_bbox_req.preprocessed_ray.w_x,
    s3_bbox_req.rid
};
assign bbox_req_stream_rsc_vld = (s3_output_type == `BBOX_REQ);

assign ist_mem_req_stream_rsc_dat = {
    s3_ist_mem_req.trig_idx,
    s3_ist_mem_req.num_trigs,
    s3_ist_mem_req.rid
};
assign ist_mem_req_stream_rsc_vld = (s3_output_type == `IST_MEM_REQ);

assign ist_req_stream_rsc_dat = {
    s3_ist_req.trig.e2_z,
    s3_ist_req.trig.e2_y,
    s3_ist_req.trig.e2_x,
    s3_ist_req.trig.e1_z,
    s3_ist_req.trig.e1_y,
    s3_ist_req.trig.e1_x,
    s3_ist_req.trig.p0_z,
    s3_ist_req.trig.p0_y,
    s3_ist_req.trig.p0_x,
    s3_ist_req.ray.tmax,
    s3_ist_req.ray.tmin,
    s3_ist_req.ray.dir_z,
    s3_ist_req.ray.dir_y,
    s3_ist_req.ray.dir_x,
    s3_ist_req.ray.origin_z,
    s3_ist_req.ray.origin_y,
    s3_ist_req.ray.origin_x,
    s3_ist_req.rid
};
assign ist_req_stream_rsc_vld = (s3_output_type == `IST_REQ);

assign trv_resp_stream_rsc_dat = {
    s3_trv_resp.result.v,
    s3_trv_resp.result.u,
    s3_trv_resp.result.t,
    s3_trv_resp.result.intersected,
    s3_trv_resp.rid
};
assign trv_resp_stream_rsc_vld = (s3_output_type == `TRV_RESP);

// global ////////////////////////////////////////

always_ff @(posedge clk or negedge arst_n) begin
    if (~arst_n)
        state <= S2;
    else
        state <= next_state;
end

always_comb begin
    next_state = state;
    case (state)
        S2: begin
            if (s2_input_type != 0)
                next_state = S3;
        end
        S3: begin
            if (s3_input_type != 0)
                next_state = S2;
        end
    endcase
end

always_comb begin
    input_type = 0;
    if (trv_req_stream_rsc_vld)
        input_type = `TRV_REQ;
    else if (bbox_mem_resp_stream_rsc_vld)
        input_type = `BBOX_MEM_RESP;
    else if (bbox_resp_stream_rsc_vld)
        input_type = `BBOX_RESP;
    else if (ist_mem_resp_stream_rsc_vld)
        input_type = `IST_MEM_RESP;
    else if (ist_resp_stream_rsc_vld)
        input_type = `IST_RESP;
end

always_comb begin
    input_reg = 0;
    case (input_type)
        // verilator lint_off WIDTHEXPAND
        `TRV_REQ:       input_reg = trv_req_stream_rsc_dat;
        `BBOX_MEM_RESP: input_reg = bbox_mem_resp_stream_rsc_dat;
        `BBOX_RESP:     input_reg = bbox_resp_stream_rsc_dat;
        `IST_MEM_RESP:  input_reg = ist_mem_resp_stream_rsc_dat;
        `IST_RESP:      input_reg = ist_resp_stream_rsc_dat;
        // verilator lint_on WIDTHEXPAND
    endcase
end

assign stall = 0;

always_comb begin
    local_mem_a_nxt = 0;
    if (s3_pop_node_stack)
        local_mem_a_nxt.node_stack_size = s3_local_mem_node_stack_size_decr;
    else if (s3_input_type == `TRV_REQ)
        local_mem_a_nxt.node_stack_size = 0;
    else if (s3_push_node_stack)
        local_mem_a_nxt.node_stack_size = s3_local_mem_node_stack_size_incr;
end

always_comb begin
    local_mem_b_nxt = 0;
    local_mem_c_nxt = 0;
    local_mem_d_nxt = 0;
    if (s3_input_type == `TRV_REQ) begin
        local_mem_b_nxt.origin_x = s3_trv_req.all_ray.origin_x;
        local_mem_b_nxt.origin_y = s3_trv_req.all_ray.origin_y;
        local_mem_b_nxt.origin_z = s3_trv_req.all_ray.origin_z;
        local_mem_b_nxt.dir_x    = s3_trv_req.all_ray.dir_x;
        local_mem_b_nxt.dir_y    = s3_trv_req.all_ray.dir_y;
        local_mem_b_nxt.dir_z    = s3_trv_req.all_ray.dir_z;
        local_mem_c_nxt.w_x      = s3_trv_req.all_ray.w_x;
        local_mem_c_nxt.w_y      = s3_trv_req.all_ray.w_y;
        local_mem_c_nxt.w_z      = s3_trv_req.all_ray.w_z;
        local_mem_d_nxt.tmin     = s3_trv_req.all_ray.tmin;
    end
end

always_comb begin
    local_mem_e_nxt.tmax = s3_local_mem_e.tmax;
    local_mem_f_nxt.intersected = s3_local_mem_f.intersected;
    local_mem_f_nxt.u = s3_local_mem_f.u;
    local_mem_f_nxt.v = s3_local_mem_f.v;
    if (s3_input_type == `TRV_REQ) begin
        local_mem_e_nxt.tmax = s3_trv_req.all_ray.tmax;
        local_mem_f_nxt.intersected = 0;
        local_mem_f_nxt.u = 0;
        local_mem_f_nxt.v = 0;
    end else if (s3_input_type == `IST_RESP && s3_ist_resp.intersected) begin
        local_mem_e_nxt.tmax = s3_ist_resp.t;
        local_mem_f_nxt.intersected = 1;
        local_mem_f_nxt.u = s3_ist_resp.u;
        local_mem_f_nxt.v = s3_ist_resp.v;
    end
end

always_comb begin
    local_mem_g_nxt = 0;
    if (s3_input_type == `BBOX_MEM_RESP) begin
        local_mem_g_nxt.left_node = s3_bbox_mem_resp.nbp.left_node;
        local_mem_g_nxt.right_node = s3_bbox_mem_resp.nbp.right_node;
    end
end

always_comb begin
    local_mem_h_nxt = 0;
    if (s3_send_mem_req_ist)
        local_mem_h_nxt.num_trigs_left = s3_send_mem_req_node.num_trigs;
    else if (s3_output_type == `IST_MEM_RESP || s3_output_type == `IST_RESP)
        local_mem_h_nxt.num_trigs_left = s3_local_mem_num_trigs_left_decr;
end

always_comb begin
    local_mem_i_nxt = 0;
    if (s3_pop_node_stack) begin
        local_mem_i_nxt.b = s3_node_stack_top_b;
    end else if (s3_input_type == `TRV_REQ) begin
        local_mem_i_nxt.b.x_min = s3_trv_req.all_ray.b_x;
        local_mem_i_nxt.b.y_min = s3_trv_req.all_ray.b_y;
        local_mem_i_nxt.b.z_min = s3_trv_req.all_ray.b_z;
    end else if (s3_input_type == `BBOX_RESP) begin
        if (s3_bbox_resp.left_hit) begin
            if (s3_bbox_resp.right_hit)
                local_mem_i_nxt.b = s3_first_b;
            else
                local_mem_i_nxt.b = s3_bbox_resp.left_b;
        end else if (s3_bbox_resp.right_hit) begin
            local_mem_i_nxt.b = s3_bbox_resp.right_b;
        end
    end
end

// stage 2 ///////////////////////////////////////

assign s2_input_type = state == S2 ? input_type : 0;
assign s2_input_reg  = state == S2 ? input_reg : 0;

assign s2_cid = s2_input_reg[`TID_WIDTH+:`CID_WIDTH];

assign s2_local_mem_a = local_mem_a_rd_dout;
assign s2_local_mem_h = local_mem_h_rd_dout;

assign s2_local_mem_node_stack_size_decr = s2_local_mem_a.node_stack_size - 1'b1;
assign s2_local_mem_num_trigs_left_decr = s2_local_mem_h.num_trigs_left - 1'b1;

// verilator lint_off WIDTHTRUNC
assign {
    s2_bbox_resp.left_first,
    s2_bbox_resp.right_hit,
    s2_bbox_resp.left_hit,
    s2_bbox_resp.right_b.z_min,
    s2_bbox_resp.right_b.y_min,
    s2_bbox_resp.right_b.x_min,
    s2_bbox_resp.left_b.z_min,
    s2_bbox_resp.left_b.y_min,
    s2_bbox_resp.left_b.x_min,
    s2_bbox_resp.rid
} = s2_input_reg;
// verilator lint_on WIDTHTRUNC

assign s2_send_ist_req = s2_input_type == `IST_MEM_RESP || (s2_input_type == `IST_RESP && s2_local_mem_h.num_trigs_left != 0);

assign s2_stack_op = (s2_input_type == `BBOX_RESP && (~s2_bbox_resp.left_hit) && (~s2_bbox_resp.right_hit)) || (s2_input_type == `IST_RESP && s2_local_mem_h.num_trigs_left == 0);
assign s2_send_trv_resp = s2_stack_op && s2_local_mem_a.node_stack_size == 0;
assign s2_pop_node_stack = s2_stack_op && s2_local_mem_a.node_stack_size != 0;

// stage 3 ///////////////////////////////////////

assign s3_input_type = state == S3 ? input_type : 0;
assign s3_input_reg  = state == S3 ? input_reg : 0;

assign s3_rid = s3_input_reg[`RID_WIDTH-1:0];
assign s3_cid = s3_input_reg[`TID_WIDTH+:`CID_WIDTH];

assign s3_local_mem_a = local_mem_a_rd_dout;
assign s3_local_mem_b = local_mem_b_rd_dout;
assign s3_local_mem_c = local_mem_c_rd_dout;
assign s3_local_mem_d = local_mem_d_rd_dout;
assign s3_local_mem_e = local_mem_e_rd_dout;
assign s3_local_mem_f = local_mem_f_rd_dout;
assign s3_local_mem_g = local_mem_g_rd_dout;
assign s3_local_mem_h = local_mem_h_rd_dout;
assign s3_local_mem_i = local_mem_i_rd_dout;

assign s3_local_mem_node_stack_size_incr = s3_local_mem_a.node_stack_size + 1'b1;
assign s3_local_mem_node_stack_size_decr = s3_local_mem_a.node_stack_size - 1'b1;
assign s3_local_mem_num_trigs_left_decr = s3_local_mem_h.num_trigs_left - 1'b1;

// verilator lint_off WIDTHTRUNC
assign {
    s3_trv_req.all_ray.tmax,
    s3_trv_req.all_ray.tmin,
    s3_trv_req.all_ray.b_z,
    s3_trv_req.all_ray.b_y,
    s3_trv_req.all_ray.b_x,
    s3_trv_req.all_ray.w_z,
    s3_trv_req.all_ray.w_y,
    s3_trv_req.all_ray.w_x,
    s3_trv_req.all_ray.dir_z,
    s3_trv_req.all_ray.dir_y,
    s3_trv_req.all_ray.dir_x,
    s3_trv_req.all_ray.origin_z,
    s3_trv_req.all_ray.origin_y,
    s3_trv_req.all_ray.origin_x,
    s3_trv_req.rid
} = s3_input_reg;

assign {
    s3_bbox_mem_resp.nbp.right_bbox.z_max,
    s3_bbox_mem_resp.nbp.right_bbox.z_min,
    s3_bbox_mem_resp.nbp.right_bbox.y_max,
    s3_bbox_mem_resp.nbp.right_bbox.y_min,
    s3_bbox_mem_resp.nbp.right_bbox.x_max,
    s3_bbox_mem_resp.nbp.right_bbox.x_min,
    s3_bbox_mem_resp.nbp.left_bbox.z_max,
    s3_bbox_mem_resp.nbp.left_bbox.z_min,
    s3_bbox_mem_resp.nbp.left_bbox.y_max,
    s3_bbox_mem_resp.nbp.left_bbox.y_min,
    s3_bbox_mem_resp.nbp.left_bbox.x_max,
    s3_bbox_mem_resp.nbp.left_bbox.x_min,
    s3_bbox_mem_resp.nbp.exp.z,
    s3_bbox_mem_resp.nbp.exp.y,
    s3_bbox_mem_resp.nbp.exp.x,
    s3_bbox_mem_resp.nbp.right_node.child_idx,
    s3_bbox_mem_resp.nbp.right_node.num_trigs,
    s3_bbox_mem_resp.nbp.left_node.child_idx,
    s3_bbox_mem_resp.nbp.left_node.num_trigs,
    s3_bbox_mem_resp.rid
} = s3_input_reg;

assign {
    s3_bbox_resp.left_first,
    s3_bbox_resp.right_hit,
    s3_bbox_resp.left_hit,
    s3_bbox_resp.right_b.z_min,
    s3_bbox_resp.right_b.y_min,
    s3_bbox_resp.right_b.x_min,
    s3_bbox_resp.left_b.z_min,
    s3_bbox_resp.left_b.y_min,
    s3_bbox_resp.left_b.x_min,
    s3_bbox_resp.rid
} = s3_input_reg;

assign s3_ist_mem_resp.rid = s3_input_reg;

assign {
    s3_ist_resp.v,
    s3_ist_resp.u,
    s3_ist_resp.t,
    s3_ist_resp.intersected,
    s3_ist_resp.rid
} = s3_input_reg;
// verilator lint_on WIDTHTRUNC

assign s3_trv_req_rid = s3_trv_req.rid;
assign s3_bbox_mem_resp_rid = s3_bbox_mem_resp.rid;
assign s3_bbox_resp_rid = s3_bbox_resp.rid;
assign s3_ist_mem_resp_rid = s3_ist_mem_resp.rid;
assign s3_ist_resp_rid = s3_ist_resp.rid;

assign s3_send_ist_req = s3_input_type == `IST_MEM_RESP || (s3_input_type == `IST_RESP && s3_local_mem_h.num_trigs_left != 0);

always_comb begin
    s3_send_mem_req = 0;
    s3_send_mem_req_node = 0;
    if (s3_pop_node_stack) begin
        s3_send_mem_req = 1;
        s3_send_mem_req_node = s3_node_stack_top_node;
    end else if (s3_input_type == `BBOX_RESP) begin
        if (s3_bbox_resp.left_hit) begin
            if (s3_bbox_resp.right_hit) begin
                s3_send_mem_req = 1;
                s3_send_mem_req_node = s3_first_node;
            end else begin
                s3_send_mem_req = 1;
                s3_send_mem_req_node = s3_local_mem_g.left_node;
            end
        end else if (s3_bbox_resp.right_hit) begin
            s3_send_mem_req = 1;
            s3_send_mem_req_node = s3_local_mem_g.right_node;
        end
    end
end

always_comb begin
    s3_send_mem_req_ist = 0;
    s3_send_mem_req_output_type = 0;
    if (s3_send_mem_req) begin
        if (s3_send_mem_req_node.num_trigs == 0) begin
            s3_send_mem_req_output_type = `BBOX_MEM_REQ;
        end else begin
            s3_send_mem_req_ist = 1;
            s3_send_mem_req_output_type = `IST_MEM_REQ;
        end
    end
end

assign s3_stack_op = (s3_input_type == `BBOX_RESP && (~s3_bbox_resp.left_hit) && (~s3_bbox_resp.right_hit)) || (s3_input_type == `IST_RESP && s3_local_mem_h.num_trigs_left == 0);
assign s3_pop_node_stack = s3_stack_op && s3_local_mem_a.node_stack_size != 0;
assign {s3_node_stack_top_node, s3_node_stack_top_b} = node_stack_rd_dout;
assign s3_stack_op_output_type = s3_local_mem_a.node_stack_size == 0 ? `TRV_RESP : s3_send_mem_req_output_type;

assign s3_push_node_stack = s3_input_type == `BBOX_RESP && s3_bbox_resp.left_hit && s3_bbox_resp.right_hit;

assign s3_left_is_leaf = s3_local_mem_g.left_node.num_trigs != 0;
assign s3_right_is_leaf = s3_local_mem_g.right_node.num_trigs != 0;
assign s3_left_first = s3_left_is_leaf || (!s3_right_is_leaf && s3_bbox_resp.left_first);
assign s3_first_node = s3_left_first ? s3_local_mem_g.left_node : s3_local_mem_g.right_node;
assign s3_second_node = s3_left_first ? s3_local_mem_g.right_node : s3_local_mem_g.left_node;
assign s3_first_b = s3_left_first ? s3_bbox_resp.left_b : s3_bbox_resp.right_b;
assign s3_second_b = s3_left_first ? s3_bbox_resp.right_b : s3_bbox_resp.left_b;

always_comb begin
    s3_bbox_mem_req.rid = s3_rid;
    s3_bbox_mem_req.nbp_idx = (s3_input_type == `TRV_REQ) ? 0 : s3_send_mem_req_node.child_idx;
end

always_comb begin
    s3_bbox_req.rid                   = s3_rid;
    s3_bbox_req.preprocessed_ray.w_x  = s3_local_mem_c.w_x;
    s3_bbox_req.preprocessed_ray.w_y  = s3_local_mem_c.w_y;
    s3_bbox_req.preprocessed_ray.w_z  = s3_local_mem_c.w_z;
    s3_bbox_req.preprocessed_ray.b_x  = s3_local_mem_i.b.x_min;
    s3_bbox_req.preprocessed_ray.b_y  = s3_local_mem_i.b.y_min;
    s3_bbox_req.preprocessed_ray.b_z  = s3_local_mem_i.b.z_min;
    s3_bbox_req.preprocessed_ray.tmin = s3_local_mem_d.tmin;
    s3_bbox_req.preprocessed_ray.tmax = s3_local_mem_e.tmax;
    s3_bbox_req.exp                   = s3_bbox_mem_resp.nbp.exp;
    s3_bbox_req.left_bbox             = s3_bbox_mem_resp.nbp.left_bbox;
    s3_bbox_req.right_bbox            = s3_bbox_mem_resp.nbp.right_bbox;
end

always_comb begin
    s3_ist_mem_req.rid       = s3_rid;
    s3_ist_mem_req.num_trigs = s3_send_mem_req_node.num_trigs;
    s3_ist_mem_req.trig_idx  = s3_send_mem_req_node.child_idx;
end

always_comb begin
    s3_ist_req.rid          = s3_rid;
    s3_ist_req.ray.origin_x = s3_local_mem_b.origin_x;
    s3_ist_req.ray.origin_y = s3_local_mem_b.origin_y;
    s3_ist_req.ray.origin_z = s3_local_mem_b.origin_z;
    s3_ist_req.ray.dir_x    = s3_local_mem_b.dir_x;
    s3_ist_req.ray.dir_y    = s3_local_mem_b.dir_y;
    s3_ist_req.ray.dir_z    = s3_local_mem_b.dir_z;
    s3_ist_req.ray.tmin     = s3_local_mem_d.tmin;
    s3_ist_req.ray.tmax     = local_mem_e_nxt.tmax;
    {   
        s3_ist_req.trig.e2_z,
        s3_ist_req.trig.e2_y,
        s3_ist_req.trig.e2_x,
        s3_ist_req.trig.e1_z,
        s3_ist_req.trig.e1_y,
        s3_ist_req.trig.e1_x,
        s3_ist_req.trig.p0_z,
        s3_ist_req.trig.p0_y,
        s3_ist_req.trig.p0_x
    } = trig_sram_rd_dout;
end

always_comb begin
    s3_trv_resp.rid                = s3_rid;
    s3_trv_resp.result.intersected = local_mem_f_nxt.intersected;
    s3_trv_resp.result.t           = local_mem_e_nxt.tmax;
    s3_trv_resp.result.u           = local_mem_f_nxt.u;
    s3_trv_resp.result.v           = local_mem_f_nxt.v;
end

always_comb begin
    s3_output_type = 0;
    case (s3_input_type)
        `TRV_REQ: s3_output_type = `BBOX_MEM_REQ;
        `BBOX_MEM_RESP: s3_output_type = `BBOX_REQ;
        `BBOX_RESP: begin
            if (s3_bbox_resp.left_hit || s3_bbox_resp.right_hit)
                s3_output_type = s3_send_mem_req_output_type;
            else
                s3_output_type = s3_stack_op_output_type;
        end
        `IST_MEM_RESP: s3_output_type = `IST_REQ;
        `IST_RESP: begin
            if (s3_local_mem_h.num_trigs_left == 0)
                s3_output_type = s3_stack_op_output_type;
            else
                s3_output_type = `IST_REQ;
        end
    endcase
end

endmodule
