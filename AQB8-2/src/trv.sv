`timescale 1ns / 1ps
`include "../include/datatypes.svh"

`define MAX(a, b)        ((a) > (b) ? (a) : (b))

`define INPUT_REG_WIDTH  `MAX(`TRV_REQ_WIDTH, `MAX(`CLSTR_MEM_RESP_WIDTH, `MAX(`CLSTR_RESP_WIDTH, `MAX(`UPDT_RESP_WIDTH, `MAX(`BBOX_MEM_RESP_WIDTH, `MAX(`BBOX_RESP_WIDTH, `MAX(`IST_MEM_RESP_WIDTH, `IST_RESP_WIDTH)))))))

`define TRV_REQ        1
`define CLSTR_MEM_RESP 2
`define CLSTR_RESP     3
`define UPDT_RESP      4
`define BBOX_MEM_RESP  5
`define BBOX_RESP      6
`define IST_MEM_RESP   7
`define IST_RESP       8

`define CLSTR_MEM_REQ 1
`define CLSTR_REQ     2
`define UPDT_REQ      3
`define BBOX_MEM_REQ  4
`define BBOX_REQ      5
`define IST_MEM_REQ   6
`define IST_REQ       7
`define TRV_RESP      8

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

    input  [ `CLSTR_MEM_RESP_WIDTH-1:0] clstr_mem_resp_stream_rsc_dat,
    input                               clstr_mem_resp_stream_rsc_vld,
    output                              clstr_mem_resp_stream_rsc_rdy,

    input  [     `CLSTR_RESP_WIDTH-1:0] clstr_resp_stream_rsc_dat,
    input                               clstr_resp_stream_rsc_vld,
    output                              clstr_resp_stream_rsc_rdy,

    input  [      `UPDT_RESP_WIDTH-1:0] updt_resp_stream_rsc_dat,
    input                               updt_resp_stream_rsc_vld,
    output                              updt_resp_stream_rsc_rdy,

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

    output [  `CLSTR_MEM_REQ_WIDTH-1:0] clstr_mem_req_stream_rsc_dat,
    output                              clstr_mem_req_stream_rsc_vld,
    input                               clstr_mem_req_stream_rsc_rdy,

    output [      `CLSTR_REQ_WIDTH-1:0] clstr_req_stream_rsc_dat,
    output                              clstr_req_stream_rsc_vld,
    input                               clstr_req_stream_rsc_rdy,

    output [       `UPDT_REQ_WIDTH-1:0] updt_req_stream_rsc_dat,
    output                              updt_req_stream_rsc_vld,
    input                               updt_req_stream_rsc_rdy,

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

logic                              local_mem_j_wr;
logic [    `LOCAL_MEM_J_WIDTH-1:0] local_mem_j_wr_din;
logic [    `LOCAL_MEM_J_WIDTH-1:0] local_mem_j_rd_dout;

logic                              local_mem_k_wr;
logic [    `LOCAL_MEM_K_WIDTH-1:0] local_mem_k_wr_din;
logic [    `LOCAL_MEM_K_WIDTH-1:0] local_mem_k_rd_dout;

logic                              local_mem_l_wr;
logic [    `LOCAL_MEM_L_WIDTH-1:0] local_mem_l_wr_din;
logic [    `LOCAL_MEM_L_WIDTH-1:0] local_mem_l_rd_dout;

logic                              local_mem_m_wr;
logic [    `LOCAL_MEM_M_WIDTH-1:0] local_mem_m_wr_din;
logic [    `LOCAL_MEM_M_WIDTH-1:0] local_mem_m_rd_dout;

logic                              local_mem_n_wr;
logic [    `LOCAL_MEM_N_WIDTH-1:0] local_mem_n_wr_din;
logic [    `LOCAL_MEM_N_WIDTH-1:0] local_mem_n_rd_dout;

logic                              local_mem_o_wr;
logic [    `LOCAL_MEM_O_WIDTH-1:0] local_mem_o_wr_din;
logic [    `LOCAL_MEM_O_WIDTH-1:0] local_mem_o_rd_dout;

logic                              local_mem_p_wr;
logic [    `LOCAL_MEM_P_WIDTH-1:0] local_mem_p_wr_din;
logic [    `LOCAL_MEM_P_WIDTH-1:0] local_mem_p_rd_dout;

logic                              local_mem_q_wr;
logic [    `LOCAL_MEM_Q_WIDTH-1:0] local_mem_q_wr_din;
logic [    `LOCAL_MEM_Q_WIDTH-1:0] local_mem_q_rd_dout;

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

reg_module #(`LOCAL_MEM_J_WIDTH) local_mem_j (
    .clk(clk),
    .arst_n(arst_n),
    .WE(local_mem_j_wr),
    .D(local_mem_j_wr_din),
    .Q(local_mem_j_rd_dout)
);

reg_module #(`LOCAL_MEM_K_WIDTH) local_mem_k (
    .clk(clk),
    .arst_n(arst_n),
    .WE(local_mem_k_wr),
    .D(local_mem_k_wr_din),
    .Q(local_mem_k_rd_dout)
);

reg_module #(`LOCAL_MEM_L_WIDTH) local_mem_l (
    .clk(clk),
    .arst_n(arst_n),
    .WE(local_mem_l_wr),
    .D(local_mem_l_wr_din),
    .Q(local_mem_l_rd_dout)
);

reg_module #(`LOCAL_MEM_M_WIDTH) local_mem_m (
    .clk(clk),
    .arst_n(arst_n),
    .WE(local_mem_m_wr),
    .D(local_mem_m_wr_din),
    .Q(local_mem_m_rd_dout)
);

reg_module #(`LOCAL_MEM_N_WIDTH) local_mem_n (
    .clk(clk),
    .arst_n(arst_n),
    .WE(local_mem_n_wr),
    .D(local_mem_n_wr_din),
    .Q(local_mem_n_rd_dout)
);

reg_module #(`LOCAL_MEM_O_WIDTH) local_mem_o (
    .clk(clk),
    .arst_n(arst_n),
    .WE(local_mem_o_wr),
    .D(local_mem_o_wr_din),
    .Q(local_mem_o_rd_dout)
);

reg_module #(`LOCAL_MEM_P_WIDTH) local_mem_p (
    .clk(clk),
    .arst_n(arst_n),
    .WE(local_mem_p_wr),
    .D(local_mem_p_wr_din),
    .Q(local_mem_p_rd_dout)
);

reg_module #(`LOCAL_MEM_Q_WIDTH) local_mem_q (
    .clk(clk),
    .arst_n(arst_n),
    .WE(local_mem_q_wr),
    .D(local_mem_q_wr_din),
    .Q(local_mem_q_rd_dout)
);

// global ////////////////////////////////////////

localparam S2 = 0,
           S3 = 1;
logic [0:0] state;
logic [0:0] next_state;

logic [                 3:0] input_type;
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
local_mem_j_t local_mem_j_nxt;
local_mem_k_t local_mem_k_nxt;
local_mem_l_t local_mem_l_nxt;
local_mem_m_t local_mem_m_nxt;
local_mem_n_t local_mem_n_nxt;
local_mem_o_t local_mem_o_nxt;
local_mem_p_t local_mem_p_nxt;
local_mem_q_t local_mem_q_nxt;

// stage 2 ///////////////////////////////////////

logic [                 3:0] s2_input_type;
logic [`INPUT_REG_WIDTH-1:0] s2_input_reg;

logic [`CID_WIDTH-1:0] s2_cid;

local_mem_a_t s2_local_mem_a;
local_mem_b_t s2_local_mem_b;
local_mem_n_t s2_local_mem_n;

stack_size_t s2_local_mem_node_stack_size_decr;
field_b_t    s2_local_mem_num_trigs_left_decr;

// verilator lint_off UNUSEDSIGNAL
clstr_resp_t s2_clstr_resp;
bbox_resp_t  s2_bbox_resp;
// verilator lint_on UNUSEDSIGNAL

logic s2_send_ist_req;

logic s2_send_mem_req;

logic s2_stack_op;
logic s2_send_trv_resp;
logic s2_peek_node_stack;

logic s2_push_node_stack;

// stage 3 ///////////////////////////////////////

logic [                 3:0] s3_input_type;
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
local_mem_j_t s3_local_mem_j;
local_mem_k_t s3_local_mem_k;
local_mem_l_t s3_local_mem_l;
local_mem_m_t s3_local_mem_m;
local_mem_n_t s3_local_mem_n;
local_mem_o_t s3_local_mem_o;
local_mem_p_t s3_local_mem_p;
local_mem_q_t s3_local_mem_q;

stack_size_t s3_local_mem_node_stack_size_incr;
stack_size_t s3_local_mem_node_stack_size_decr;
child_idx_t  s3_local_mem_nbp_offset_add_s3_send_mem_req_node_field_c;
child_idx_t  s3_local_mem_trig_offset_add_s3_send_mem_req_node_field_c;
field_b_t    s3_local_mem_num_trigs_left_decr;

trv_req_t        s3_trv_req;
clstr_mem_resp_t s3_clstr_mem_resp;
clstr_resp_t     s3_clstr_resp;
updt_resp_t      s3_updt_resp;
bbox_mem_resp_t  s3_bbox_mem_resp;
bbox_resp_t      s3_bbox_resp;
ist_mem_resp_t   s3_ist_mem_resp;
ist_resp_t       s3_ist_resp;

// verilator lint_off UNUSEDSIGNAL
rid_t s3_trv_req_rid;
rid_t s3_clstr_mem_resp_rid;
rid_t s3_clstr_resp_rid;
rid_t s3_updt_resp_rid;
rid_t s3_bbox_mem_resp_rid;
rid_t s3_bbox_resp_rid;
rid_t s3_ist_mem_resp_rid;
rid_t s3_ist_resp_rid;
// verilator lint_on UNUSEDSIGNAL

logic s3_send_ist_req;

logic         s3_send_clstr_mem_req;
cluster_idx_t s3_send_clstr_mem_req_cluster_idx;

child_idx_t s3_send_bbox_mem_req_nbp_idx;

field_b_t   s3_send_ist_mem_req_num_trigs;
child_idx_t s3_send_ist_mem_req_trig_idx;

logic       s3_send_mem_req;
node_t      s3_send_mem_req_node;
logic       s3_send_mem_req_clstr;
logic       s3_send_mem_req_bbox;
logic       s3_send_mem_req_ist;
logic [3:0] s3_send_mem_req_output_type;

logic             s3_stack_op;
logic             s3_peek_node_stack;
logic             s3_reintersect;
logic             s3_recompute_qymax;
logic             s3_pop_node_stack;
node_stack_data_t s3_node_stack_top;
logic [3:0]       s3_stack_op_output_type;

logic s3_push_node_stack;

logic  s3_left_is_leaf;
logic  s3_right_is_leaf;
logic  s3_left_first;
node_t s3_first_node;
node_t s3_second_node;

clstr_mem_req_t s3_clstr_mem_req;
clstr_req_t     s3_clstr_req;
updt_req_t      s3_updt_req;
bbox_mem_req_t  s3_bbox_mem_req;
bbox_req_t      s3_bbox_req;
ist_mem_req_t   s3_ist_mem_req;
ist_req_t       s3_ist_req;
trv_resp_t      s3_trv_resp;

logic [3:0] s3_output_type;

// io ////////////////////////////////////////////

assign trv_req_stream_rsc_rdy        = (~stall) && (s3_input_type == `TRV_REQ);
assign clstr_mem_resp_stream_rsc_rdy = (~stall) && (s3_input_type == `CLSTR_MEM_RESP);
assign clstr_resp_stream_rsc_rdy     = (~stall) && (s3_input_type == `CLSTR_RESP);
assign updt_resp_stream_rsc_rdy      = (~stall) && (s3_input_type == `UPDT_RESP);
assign bbox_mem_resp_stream_rsc_rdy  = (~stall) && (s3_input_type == `BBOX_MEM_RESP);
assign bbox_resp_stream_rsc_rdy      = (~stall) && (s3_input_type == `BBOX_RESP);
assign ist_mem_resp_stream_rsc_rdy   = (~stall) && (s3_input_type == `IST_MEM_RESP);
assign ist_resp_stream_rsc_rdy       = (~stall) && (s3_input_type == `IST_RESP);

assign trig_sram_rd      = (~stall) && s2_send_ist_req;
assign trig_sram_rd_addr = {s2_local_mem_num_trigs_left_decr, s2_cid};

assign node_stack_wr      = (~stall) && s3_push_node_stack;
assign node_stack_wr_addr = {s3_local_mem_a.node_stack_size, s3_cid};
assign node_stack_wr_din  = {s3_second_node, s3_local_mem_c.cluster_idx};
assign node_stack_rd      = (~stall) && s2_peek_node_stack;
assign node_stack_rd_addr = {s2_local_mem_node_stack_size_decr, s2_cid};

assign local_mem_a_wr      = (~stall) && (s3_pop_node_stack || s3_input_type == `TRV_REQ || s3_push_node_stack);
assign local_mem_a_wr_din  = local_mem_a_nxt;

assign local_mem_b_wr      = (~stall) && (s3_reintersect || s3_input_type == `TRV_REQ || s3_input_type == `CLSTR_RESP);
assign local_mem_b_wr_din  = local_mem_b_nxt;

assign local_mem_c_wr      = (~stall) && s3_send_clstr_mem_req;
assign local_mem_c_wr_din  = local_mem_c_nxt;

assign local_mem_d_wr      = (~stall) && s3_input_type == `CLSTR_MEM_RESP;
assign local_mem_d_wr_din  = local_mem_d_nxt;

assign local_mem_e_wr      = local_mem_d_wr;
assign local_mem_e_wr_din  = local_mem_e_nxt;

assign local_mem_f_wr      = (~stall) && (s3_input_type == `CLSTR_RESP && (s3_clstr_resp.intersected || s3_local_mem_b.reintersect));
assign local_mem_f_wr_din  = local_mem_f_nxt;

assign local_mem_g_wr      = (~stall) && s3_input_type == `TRV_REQ;
assign local_mem_g_wr_din  = local_mem_g_nxt;

assign local_mem_h_wr      = local_mem_g_wr;
assign local_mem_h_wr_din  = local_mem_h_nxt;

assign local_mem_i_wr      = local_mem_g_wr;
assign local_mem_i_wr_din  = local_mem_i_nxt;

assign local_mem_j_wr      = local_mem_g_wr;
assign local_mem_j_wr_din  = local_mem_j_nxt;

assign local_mem_k_wr      = (~stall) && (s3_input_type == `TRV_REQ || (s3_input_type == `IST_RESP && s3_ist_resp.intersected));
assign local_mem_k_wr_din  = local_mem_k_nxt;

assign local_mem_l_wr      = local_mem_k_wr;
assign local_mem_l_wr_din  = local_mem_l_nxt;

assign local_mem_m_wr      = (~stall) && (s3_input_type == `BBOX_MEM_RESP);
assign local_mem_m_wr_din  = local_mem_m_nxt;

assign local_mem_n_wr      = (~stall) && (s3_send_ist_req || s3_send_mem_req_ist);
assign local_mem_n_wr_din  = local_mem_n_nxt;

assign local_mem_o_wr      = (~stall) && (s3_input_type == `TRV_REQ || s3_input_type == `CLSTR_RESP || s3_input_type == `UPDT_RESP || (s3_input_type == `IST_RESP && s3_ist_resp.intersected));
assign local_mem_o_wr_din  = local_mem_o_nxt;

assign local_mem_p_wr      = local_mem_f_wr;
assign local_mem_p_wr_din  = local_mem_p_nxt;

assign local_mem_q_wr      = (~stall) && ((s3_input_type == `CLSTR_RESP && (s3_clstr_resp.intersected || s3_local_mem_b.reintersect)) || s3_input_type == `UPDT_RESP);
assign local_mem_q_wr_din  = local_mem_q_nxt;

assign clstr_mem_req_stream_rsc_dat = {
    s3_clstr_mem_req.cluster_idx,
    s3_clstr_mem_req.rid
};
assign clstr_mem_req_stream_rsc_vld = (s3_output_type == `CLSTR_MEM_REQ);

assign clstr_req_stream_rsc_dat = {
    s3_clstr_req.z_max,
    s3_clstr_req.z_min,
    s3_clstr_req.y_max,
    s3_clstr_req.y_min,
    s3_clstr_req.x_max,
    s3_clstr_req.x_min,
    s3_clstr_req.inv_sx_inv_sw,
    s3_clstr_req.preprocessed_ray.tmax,
    s3_clstr_req.preprocessed_ray.tmin,
    s3_clstr_req.preprocessed_ray.b_z,
    s3_clstr_req.preprocessed_ray.b_y,
    s3_clstr_req.preprocessed_ray.b_x,
    s3_clstr_req.preprocessed_ray.w_z,
    s3_clstr_req.preprocessed_ray.w_y,
    s3_clstr_req.preprocessed_ray.w_x,
    s3_clstr_req.rid
};
assign clstr_req_stream_rsc_vld = (s3_output_type == `CLSTR_REQ);

assign updt_req_stream_rsc_dat = {
    s3_updt_req.tmax,
    s3_updt_req.y_ref,
    s3_updt_req.inv_sx_inv_sw,
    s3_updt_req.rid
};
assign updt_req_stream_rsc_vld = (s3_output_type == `UPDT_REQ);

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
    s3_bbox_req.quant_ray.qy_max,
    s3_bbox_req.quant_ray.qb_l_z,
    s3_bbox_req.quant_ray.qb_l_y,
    s3_bbox_req.quant_ray.qb_l_x,
    s3_bbox_req.quant_ray.rw_h_z,
    s3_bbox_req.quant_ray.rw_h_y,
    s3_bbox_req.quant_ray.rw_h_x,
    s3_bbox_req.quant_ray.rw_l_z,
    s3_bbox_req.quant_ray.rw_l_y,
    s3_bbox_req.quant_ray.rw_l_x,
    s3_bbox_req.quant_ray.qw_h_z,
    s3_bbox_req.quant_ray.qw_h_y,
    s3_bbox_req.quant_ray.qw_h_x,
    s3_bbox_req.quant_ray.qw_l_z,
    s3_bbox_req.quant_ray.qw_l_y,
    s3_bbox_req.quant_ray.qw_l_x,
    s3_bbox_req.quant_ray.iw_z,
    s3_bbox_req.quant_ray.iw_y,
    s3_bbox_req.quant_ray.iw_x,
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
    else if (clstr_mem_resp_stream_rsc_vld)
        input_type = `CLSTR_MEM_RESP;
    else if (clstr_resp_stream_rsc_vld)
        input_type = `CLSTR_RESP;
    else if (updt_resp_stream_rsc_vld)
        input_type = `UPDT_RESP;
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
        `TRV_REQ:        input_reg = trv_req_stream_rsc_dat;
        `CLSTR_MEM_RESP: input_reg = clstr_mem_resp_stream_rsc_dat;
        `CLSTR_RESP:     input_reg = clstr_resp_stream_rsc_dat;
        `UPDT_RESP:      input_reg = updt_resp_stream_rsc_dat;
        `BBOX_MEM_RESP:  input_reg = bbox_mem_resp_stream_rsc_dat;
        `BBOX_RESP:      input_reg = bbox_resp_stream_rsc_dat;
        `IST_MEM_RESP:   input_reg = ist_mem_resp_stream_rsc_dat;
        `IST_RESP:       input_reg = ist_resp_stream_rsc_dat;
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
    if (s3_reintersect)
        local_mem_b_nxt.reintersect = 1;
    else if (s3_input_type == `TRV_REQ || s3_input_type == `CLSTR_RESP)
        local_mem_b_nxt.reintersect = 0;
end

always_comb begin
    local_mem_c_nxt = 0;
    if (s3_send_clstr_mem_req)
        local_mem_c_nxt = s3_send_clstr_mem_req_cluster_idx;
end

always_comb begin
    local_mem_d_nxt = 0;
    local_mem_e_nxt = 0;
    if (s3_input_type == `CLSTR_MEM_RESP) begin
        local_mem_d_nxt.nbp_offset = s3_clstr_mem_resp.cluster.nbp_offset;
        local_mem_e_nxt.trig_offset = s3_clstr_mem_resp.cluster.trig_offset;
    end
end

always_comb begin
    local_mem_f_nxt = 0;
    local_mem_p_nxt = 0;
    if (s3_input_type == `CLSTR_RESP && (s3_clstr_resp.intersected || s3_local_mem_b.reintersect)) begin
        local_mem_f_nxt.qb_l_x        = s3_clstr_resp.qb_l_x;
        local_mem_f_nxt.qb_l_y        = s3_clstr_resp.qb_l_y;
        local_mem_f_nxt.qb_l_z        = s3_clstr_resp.qb_l_z;
        local_mem_p_nxt.inv_sx_inv_sw = s3_clstr_resp.inv_sx_inv_sw;
        local_mem_p_nxt.y_ref         = s3_clstr_resp.y_ref;
    end
end

always_comb begin
    local_mem_g_nxt = 0;
    local_mem_h_nxt = 0;
    local_mem_i_nxt = 0;
    local_mem_j_nxt = 0;
    if (s3_input_type == `TRV_REQ) begin
        local_mem_g_nxt.origin_x = s3_trv_req.all_ray.origin_x;
        local_mem_g_nxt.origin_y = s3_trv_req.all_ray.origin_y;
        local_mem_g_nxt.origin_z = s3_trv_req.all_ray.origin_z;
        local_mem_g_nxt.dir_x    = s3_trv_req.all_ray.dir_x;
        local_mem_g_nxt.dir_y    = s3_trv_req.all_ray.dir_y;
        local_mem_g_nxt.dir_z    = s3_trv_req.all_ray.dir_z;
        local_mem_h_nxt.w_x      = s3_trv_req.all_ray.w_x;
        local_mem_h_nxt.w_y      = s3_trv_req.all_ray.w_y;
        local_mem_h_nxt.w_z      = s3_trv_req.all_ray.w_z;
        local_mem_h_nxt.b_x      = s3_trv_req.all_ray.b_x;
        local_mem_h_nxt.b_y      = s3_trv_req.all_ray.b_y;
        local_mem_h_nxt.b_z      = s3_trv_req.all_ray.b_z;
        local_mem_i_nxt.iw_x     = s3_trv_req.all_ray.w_x[31];
        local_mem_i_nxt.iw_y     = s3_trv_req.all_ray.w_y[31];
        local_mem_i_nxt.iw_z     = s3_trv_req.all_ray.w_z[31];
        local_mem_i_nxt.qw_l_x   = s3_trv_req.all_ray.qw_l_x;
        local_mem_i_nxt.qw_l_y   = s3_trv_req.all_ray.qw_l_y;
        local_mem_i_nxt.qw_l_z   = s3_trv_req.all_ray.qw_l_z;
        local_mem_i_nxt.qw_h_x   = s3_trv_req.all_ray.qw_h_x;
        local_mem_i_nxt.qw_h_y   = s3_trv_req.all_ray.qw_h_y;
        local_mem_i_nxt.qw_h_z   = s3_trv_req.all_ray.qw_h_z;
        local_mem_i_nxt.rw_l_x   = s3_trv_req.all_ray.rw_l_x;
        local_mem_i_nxt.rw_l_y   = s3_trv_req.all_ray.rw_l_y;
        local_mem_i_nxt.rw_l_z   = s3_trv_req.all_ray.rw_l_z;
        local_mem_i_nxt.rw_h_x   = s3_trv_req.all_ray.rw_h_x;
        local_mem_i_nxt.rw_h_y   = s3_trv_req.all_ray.rw_h_y;
        local_mem_i_nxt.rw_h_z   = s3_trv_req.all_ray.rw_h_z;
        local_mem_j_nxt.tmin     = s3_trv_req.all_ray.tmin;
    end
end

always_comb begin
    local_mem_k_nxt.tmax        = s3_local_mem_k.tmax;
    local_mem_l_nxt.intersected = s3_local_mem_l.intersected;
    local_mem_l_nxt.u           = s3_local_mem_l.u;
    local_mem_l_nxt.v           = s3_local_mem_l.v;
    if (s3_input_type == `TRV_REQ) begin
        local_mem_k_nxt.tmax        = s3_trv_req.all_ray.tmax;
        local_mem_l_nxt.intersected = 0;
        local_mem_l_nxt.u           = 0;
        local_mem_l_nxt.v           = 0;
    end else if (s3_input_type == `IST_RESP && s3_ist_resp.intersected) begin
        local_mem_k_nxt.tmax        = s3_ist_resp.t;
        local_mem_l_nxt.intersected = 1;
        local_mem_l_nxt.u           = s3_ist_resp.u;
        local_mem_l_nxt.v           = s3_ist_resp.v;
    end
end

always_comb begin
    local_mem_m_nxt = 0;
    if (s3_input_type == `BBOX_MEM_RESP) begin
        local_mem_m_nxt.left_node = s3_bbox_mem_resp.nbp.left_node;
        local_mem_m_nxt.right_node = s3_bbox_mem_resp.nbp.right_node;
    end
end

always_comb begin
    local_mem_n_nxt = 0;
    if (s3_send_ist_req)
        local_mem_n_nxt.num_trigs_left = s3_local_mem_num_trigs_left_decr;
    else if (s3_send_mem_req_ist)
        local_mem_n_nxt.num_trigs_left = s3_send_ist_mem_req_num_trigs;
end

always_comb begin
    local_mem_o_nxt = s3_local_mem_o;
    if (s3_input_type == `TRV_REQ || s3_input_type == `CLSTR_RESP || s3_input_type == `UPDT_RESP)
        local_mem_o_nxt.recompute_qymax = 0;
    else if (s3_input_type == `IST_RESP && s3_ist_resp.intersected)
        local_mem_o_nxt.recompute_qymax = 1;
end

always_comb begin
    local_mem_q_nxt = s3_local_mem_q;
    if (s3_input_type == `CLSTR_RESP && (s3_clstr_resp.intersected || s3_local_mem_b.reintersect))
        local_mem_q_nxt.qy_max = s3_clstr_resp.qy_max;
    else if (s3_input_type == `UPDT_RESP)
        local_mem_q_nxt.qy_max = s3_updt_resp.qy_max;
end

// stage 2 ///////////////////////////////////////

assign s2_input_type = state == S2 ? input_type : 0;
assign s2_input_reg  = state == S2 ? input_reg : 0;

assign s2_cid = s2_input_reg[`TID_WIDTH+:`CID_WIDTH];

assign s2_local_mem_a = local_mem_a_rd_dout;
assign s2_local_mem_b = local_mem_b_rd_dout;
assign s2_local_mem_n = local_mem_n_rd_dout;

assign s2_local_mem_node_stack_size_decr = s2_local_mem_a.node_stack_size - 1'b1;
assign s2_local_mem_num_trigs_left_decr = s2_local_mem_n.num_trigs_left - 1'b1;

// verilator lint_off WIDTHTRUNC
assign {
    s2_clstr_resp.qy_max,
    s2_clstr_resp.qb_l_z,
    s2_clstr_resp.qb_l_y,
    s2_clstr_resp.qb_l_x,
    s2_clstr_resp.y_ref,
    s2_clstr_resp.inv_sx_inv_sw,
    s2_clstr_resp.intersected,
    s2_clstr_resp.rid
} = s2_input_reg;

assign {
    s2_bbox_resp.left_first,
    s2_bbox_resp.right_hit,
    s2_bbox_resp.left_hit,
    s2_bbox_resp.rid
} = s2_input_reg;
// verilator lint_on WIDTHTRUNC

assign s2_send_ist_req = s2_input_type == `IST_MEM_RESP || (s2_input_type == `IST_RESP && s2_local_mem_n.num_trigs_left != 0);

assign s2_send_mem_req = s2_peek_node_stack || (s2_input_type == `BBOX_RESP && (s2_bbox_resp.left_hit || s2_bbox_resp.right_hit));

assign s2_stack_op = (s2_input_type == `CLSTR_RESP && ((~s2_clstr_resp.intersected) || s2_local_mem_b.reintersect)) || s2_input_type == `UPDT_RESP || (s2_input_type == `BBOX_RESP && (~s2_bbox_resp.left_hit) && (~s2_bbox_resp.right_hit)) || (s2_input_type == `IST_RESP && s2_local_mem_n.num_trigs_left == 0);
assign s2_send_trv_resp = s2_stack_op && s2_local_mem_a.node_stack_size == 0;
assign s2_peek_node_stack = s2_stack_op && s2_local_mem_a.node_stack_size != 0;

assign s2_push_node_stack = s2_input_type == `BBOX_RESP && s2_bbox_resp.left_hit && s2_bbox_resp.right_hit;

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
assign s3_local_mem_j = local_mem_j_rd_dout;
assign s3_local_mem_k = local_mem_k_rd_dout;
assign s3_local_mem_l = local_mem_l_rd_dout;
assign s3_local_mem_m = local_mem_m_rd_dout;
assign s3_local_mem_n = local_mem_n_rd_dout;
assign s3_local_mem_o = local_mem_o_rd_dout;
assign s3_local_mem_p = local_mem_p_rd_dout;
assign s3_local_mem_q = local_mem_q_rd_dout;

assign s3_local_mem_node_stack_size_incr = s3_local_mem_a.node_stack_size + 1'b1;
assign s3_local_mem_node_stack_size_decr = s3_local_mem_a.node_stack_size - 1'b1;
// verilator lint_off WIDTHEXPAND
assign s3_local_mem_nbp_offset_add_s3_send_mem_req_node_field_c = s3_local_mem_d.nbp_offset + s3_send_mem_req_node.field_c;
assign s3_local_mem_trig_offset_add_s3_send_mem_req_node_field_c = s3_local_mem_e.trig_offset + s3_send_mem_req_node.field_c;
// verilator lint_on WIDTHEXPAND
assign s3_local_mem_num_trigs_left_decr = s3_local_mem_n.num_trigs_left - 1'b1;

// verilator lint_off WIDTHTRUNC
assign {
    s3_trv_req.all_ray.tmax,
    s3_trv_req.all_ray.tmin,
    s3_trv_req.all_ray.rw_h_z,
    s3_trv_req.all_ray.rw_h_y,
    s3_trv_req.all_ray.rw_h_x,
    s3_trv_req.all_ray.rw_l_z,
    s3_trv_req.all_ray.rw_l_y,
    s3_trv_req.all_ray.rw_l_x,
    s3_trv_req.all_ray.qw_h_z,
    s3_trv_req.all_ray.qw_h_y,
    s3_trv_req.all_ray.qw_h_x,
    s3_trv_req.all_ray.qw_l_z,
    s3_trv_req.all_ray.qw_l_y,
    s3_trv_req.all_ray.qw_l_x,
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
    s3_clstr_mem_resp.cluster.trig_offset,
    s3_clstr_mem_resp.cluster.nbp_offset,
    s3_clstr_mem_resp.cluster.inv_sx_inv_sw,
    s3_clstr_mem_resp.cluster.z_max,
    s3_clstr_mem_resp.cluster.z_min,
    s3_clstr_mem_resp.cluster.y_max,
    s3_clstr_mem_resp.cluster.y_min,
    s3_clstr_mem_resp.cluster.x_max,
    s3_clstr_mem_resp.cluster.x_min,
    s3_clstr_mem_resp.rid
} = s3_input_reg;

assign {
    s3_clstr_resp.qy_max,
    s3_clstr_resp.qb_l_z,
    s3_clstr_resp.qb_l_y,
    s3_clstr_resp.qb_l_x,
    s3_clstr_resp.y_ref,
    s3_clstr_resp.inv_sx_inv_sw,
    s3_clstr_resp.intersected,
    s3_clstr_resp.rid
} = s3_input_reg;

assign {
    s3_updt_resp.qy_max,
    s3_updt_resp.rid
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
    s3_bbox_mem_resp.nbp.right_node.field_c,
    s3_bbox_mem_resp.nbp.right_node.field_b,
    s3_bbox_mem_resp.nbp.right_node.field_a,
    s3_bbox_mem_resp.nbp.left_node.field_c,
    s3_bbox_mem_resp.nbp.left_node.field_b,
    s3_bbox_mem_resp.nbp.left_node.field_a,
    s3_bbox_mem_resp.rid
} = s3_input_reg;

assign {
    s3_bbox_resp.left_first,
    s3_bbox_resp.right_hit,
    s3_bbox_resp.left_hit,
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
assign s3_clstr_mem_resp_rid = s3_clstr_mem_resp.rid;
assign s3_clstr_resp_rid = s3_clstr_resp.rid;
assign s3_updt_resp_rid = s3_updt_resp.rid;
assign s3_bbox_mem_resp_rid = s3_bbox_mem_resp.rid;
assign s3_bbox_resp_rid = s3_bbox_resp.rid;
assign s3_ist_mem_resp_rid = s3_ist_mem_resp.rid;
assign s3_ist_resp_rid = s3_ist_resp.rid;

assign s3_send_ist_req = s3_input_type == `IST_MEM_RESP || (s3_input_type == `IST_RESP && s3_local_mem_n.num_trigs_left != 0);

always_comb begin
    s3_send_clstr_mem_req = 0;
    s3_send_clstr_mem_req_cluster_idx = 0;
    if (s3_send_mem_req_clstr) begin
        s3_send_clstr_mem_req = 1;
        s3_send_clstr_mem_req_cluster_idx = {s3_send_mem_req_node.field_b, s3_send_mem_req_node.field_c};
    end else if (s3_reintersect) begin
        s3_send_clstr_mem_req = 1;
        s3_send_clstr_mem_req_cluster_idx = s3_node_stack_top.cluster_idx;
    end else if (s3_input_type == `TRV_REQ) begin
        s3_send_clstr_mem_req = 1;
        s3_send_clstr_mem_req_cluster_idx = 0;
    end
end

always_comb begin
    s3_send_bbox_mem_req_nbp_idx = 0;
    if (s3_send_mem_req_bbox)
        s3_send_bbox_mem_req_nbp_idx = s3_local_mem_nbp_offset_add_s3_send_mem_req_node_field_c;
    else if (s3_input_type == `CLSTR_RESP && s3_clstr_resp.intersected && (~s3_local_mem_b.reintersect))
        s3_send_bbox_mem_req_nbp_idx = s3_local_mem_d.nbp_offset;
end

assign s3_send_ist_mem_req_num_trigs = s3_send_mem_req_node.field_b;
assign s3_send_ist_mem_req_trig_idx = s3_local_mem_trig_offset_add_s3_send_mem_req_node_field_c;

always_comb begin
    s3_send_mem_req = 0;
    s3_send_mem_req_node = 0;
    if (s3_pop_node_stack) begin
        s3_send_mem_req = 1;
        s3_send_mem_req_node = s3_node_stack_top.node;
    end else if (s3_input_type == `BBOX_RESP) begin
        if (s3_bbox_resp.left_hit) begin
            if (s3_bbox_resp.right_hit) begin
                s3_send_mem_req = 1;
                s3_send_mem_req_node = s3_first_node;
            end else begin
                s3_send_mem_req = 1;
                s3_send_mem_req_node = s3_local_mem_m.left_node;
            end
        end else if (s3_bbox_resp.right_hit) begin
            s3_send_mem_req = 1;
            s3_send_mem_req_node = s3_local_mem_m.right_node;
        end
    end
end

always_comb begin
    s3_send_mem_req_clstr = 0;
    s3_send_mem_req_bbox = 0;
    s3_send_mem_req_ist = 0;
    s3_send_mem_req_output_type = 0;
    if (s3_send_mem_req) begin
        if (s3_send_mem_req_node.field_a) begin
            if (s3_send_mem_req_node.field_b == 0) begin
                s3_send_mem_req_bbox = 1;
                s3_send_mem_req_output_type = `BBOX_MEM_REQ;
            end else begin
                s3_send_mem_req_ist = 1;
                s3_send_mem_req_output_type = `IST_MEM_REQ;
            end
        end else begin
            s3_send_mem_req_clstr = 1;
            s3_send_mem_req_output_type = `CLSTR_MEM_REQ;
        end
    end
end

assign s3_stack_op = (s3_input_type == `CLSTR_RESP && ((~s3_clstr_resp.intersected) || s3_local_mem_b.reintersect)) || s3_input_type == `UPDT_RESP || (s3_input_type == `BBOX_RESP && (~s3_bbox_resp.left_hit) && (~s3_bbox_resp.right_hit)) || (s3_input_type == `IST_RESP && s3_local_mem_n.num_trigs_left == 0);
assign s3_peek_node_stack = s3_stack_op && s3_local_mem_a.node_stack_size != 0;

always_comb begin
    s3_reintersect     = 0;
    s3_recompute_qymax = 0;
    s3_pop_node_stack  = 0;
    if (s3_peek_node_stack) begin
        if (s3_node_stack_top.cluster_idx != s3_local_mem_c.cluster_idx && s3_node_stack_top.node.field_a && s3_node_stack_top.node.field_b == 0)
            s3_reintersect = 1;
        else if (local_mem_o_nxt.recompute_qymax && s3_node_stack_top.node.field_a && s3_node_stack_top.node.field_b == 0)
            s3_recompute_qymax = 1;
        else
            s3_pop_node_stack = 1;
    end
end

assign s3_node_stack_top = node_stack_rd_dout;

always_comb begin
    s3_stack_op_output_type = 0;
    if (s3_stack_op) begin
        if (s3_local_mem_a.node_stack_size == 0)
            s3_stack_op_output_type = `TRV_RESP;
        else if (s3_reintersect)
            s3_stack_op_output_type = `CLSTR_MEM_REQ;
        else if (s3_recompute_qymax)
            s3_stack_op_output_type = `UPDT_REQ;
        else
            s3_stack_op_output_type = s3_send_mem_req_output_type;
    end
end

assign s3_push_node_stack = s3_input_type == `BBOX_RESP && s3_bbox_resp.left_hit && s3_bbox_resp.right_hit;

assign s3_left_is_leaf = s3_local_mem_m.left_node.field_a && s3_local_mem_m.left_node.field_b != 0;
assign s3_right_is_leaf = s3_local_mem_m.right_node.field_a && s3_local_mem_m.right_node.field_b != 0;
assign s3_left_first = s3_left_is_leaf || (!s3_right_is_leaf && s3_bbox_resp.left_first);
assign s3_first_node = s3_left_first ? s3_local_mem_m.left_node : s3_local_mem_m.right_node;
assign s3_second_node = s3_left_first ? s3_local_mem_m.right_node : s3_local_mem_m.left_node;

always_comb begin
    s3_clstr_mem_req.rid         = s3_rid;
    s3_clstr_mem_req.cluster_idx = s3_send_clstr_mem_req_cluster_idx;
end

always_comb begin
    s3_clstr_req.rid                   = s3_rid;
    s3_clstr_req.preprocessed_ray.w_x  = s3_local_mem_h.w_x;
    s3_clstr_req.preprocessed_ray.w_y  = s3_local_mem_h.w_y;
    s3_clstr_req.preprocessed_ray.w_z  = s3_local_mem_h.w_z;
    s3_clstr_req.preprocessed_ray.b_x  = s3_local_mem_h.b_x;
    s3_clstr_req.preprocessed_ray.b_y  = s3_local_mem_h.b_y;
    s3_clstr_req.preprocessed_ray.b_z  = s3_local_mem_h.b_z;
    s3_clstr_req.preprocessed_ray.tmin = s3_local_mem_j.tmin;
    s3_clstr_req.preprocessed_ray.tmax = s3_local_mem_k.tmax;
    s3_clstr_req.inv_sx_inv_sw         = s3_clstr_mem_resp.cluster.inv_sx_inv_sw;
    s3_clstr_req.x_min                 = s3_clstr_mem_resp.cluster.x_min;
    s3_clstr_req.x_max                 = s3_clstr_mem_resp.cluster.x_max;
    s3_clstr_req.y_min                 = s3_clstr_mem_resp.cluster.y_min;
    s3_clstr_req.y_max                 = s3_clstr_mem_resp.cluster.y_max;
    s3_clstr_req.z_min                 = s3_clstr_mem_resp.cluster.z_min;
    s3_clstr_req.z_max                 = s3_clstr_mem_resp.cluster.z_max;
end

always_comb begin
    s3_updt_req.rid           = s3_rid;
    s3_updt_req.inv_sx_inv_sw = s3_local_mem_p.inv_sx_inv_sw;
    s3_updt_req.y_ref         = s3_local_mem_p.y_ref;
    s3_updt_req.tmax          = local_mem_k_nxt.tmax;
end

always_comb begin
    s3_bbox_mem_req.rid = s3_rid;
    s3_bbox_mem_req.nbp_idx = s3_send_bbox_mem_req_nbp_idx;
end

always_comb begin
    s3_bbox_req.rid              = s3_bbox_mem_resp.rid;
    s3_bbox_req.quant_ray.iw_x   = s3_local_mem_i.iw_x;
    s3_bbox_req.quant_ray.iw_y   = s3_local_mem_i.iw_y;
    s3_bbox_req.quant_ray.iw_z   = s3_local_mem_i.iw_z;
    s3_bbox_req.quant_ray.qw_l_x = s3_local_mem_i.qw_l_x;
    s3_bbox_req.quant_ray.qw_l_y = s3_local_mem_i.qw_l_y;
    s3_bbox_req.quant_ray.qw_l_z = s3_local_mem_i.qw_l_z;
    s3_bbox_req.quant_ray.qw_h_x = s3_local_mem_i.qw_h_x;
    s3_bbox_req.quant_ray.qw_h_y = s3_local_mem_i.qw_h_y;
    s3_bbox_req.quant_ray.qw_h_z = s3_local_mem_i.qw_h_z;
    s3_bbox_req.quant_ray.rw_l_x = s3_local_mem_i.rw_l_x;
    s3_bbox_req.quant_ray.rw_l_y = s3_local_mem_i.rw_l_y;
    s3_bbox_req.quant_ray.rw_l_z = s3_local_mem_i.rw_l_z;
    s3_bbox_req.quant_ray.rw_h_x = s3_local_mem_i.rw_h_x;
    s3_bbox_req.quant_ray.rw_h_y = s3_local_mem_i.rw_h_y;
    s3_bbox_req.quant_ray.rw_h_z = s3_local_mem_i.rw_h_z;
    s3_bbox_req.quant_ray.qb_l_x = s3_local_mem_f.qb_l_x;
    s3_bbox_req.quant_ray.qb_l_y = s3_local_mem_f.qb_l_y;
    s3_bbox_req.quant_ray.qb_l_z = s3_local_mem_f.qb_l_z;
    s3_bbox_req.quant_ray.qy_max = local_mem_q_nxt.qy_max;
    s3_bbox_req.left_bbox        = s3_bbox_mem_resp.nbp.left_bbox;
    s3_bbox_req.right_bbox       = s3_bbox_mem_resp.nbp.right_bbox;
end

always_comb begin
    s3_ist_mem_req.rid       = s3_rid;
    s3_ist_mem_req.num_trigs = s3_send_ist_mem_req_num_trigs;
    s3_ist_mem_req.trig_idx  = s3_send_ist_mem_req_trig_idx;
end

always_comb begin
    s3_ist_req.rid          = s3_rid;
    s3_ist_req.ray.origin_x = s3_local_mem_g.origin_x;
    s3_ist_req.ray.origin_y = s3_local_mem_g.origin_y;
    s3_ist_req.ray.origin_z = s3_local_mem_g.origin_z;
    s3_ist_req.ray.dir_x    = s3_local_mem_g.dir_x;
    s3_ist_req.ray.dir_y    = s3_local_mem_g.dir_y;
    s3_ist_req.ray.dir_z    = s3_local_mem_g.dir_z;
    s3_ist_req.ray.tmin     = s3_local_mem_j.tmin;
    s3_ist_req.ray.tmax     = local_mem_k_nxt.tmax;
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
    s3_trv_resp.result.intersected = local_mem_l_nxt.intersected;
    s3_trv_resp.result.t           = local_mem_k_nxt.tmax;
    s3_trv_resp.result.u           = local_mem_l_nxt.u;
    s3_trv_resp.result.v           = local_mem_l_nxt.v;
end

always_comb begin
    s3_output_type = 0;
    case (s3_input_type)
        `TRV_REQ: s3_output_type = `CLSTR_MEM_REQ;
        `CLSTR_MEM_RESP: s3_output_type = `CLSTR_REQ;
        `CLSTR_RESP: begin
            if (s3_clstr_resp.intersected && (~s3_local_mem_b.reintersect))
                s3_output_type = `BBOX_MEM_REQ;
            else
                s3_output_type = s3_stack_op_output_type;
        end
        `UPDT_RESP: s3_output_type = s3_stack_op_output_type;
        `BBOX_MEM_RESP: s3_output_type = `BBOX_REQ;
        `BBOX_RESP: begin
            if (s3_bbox_resp.left_hit || s3_bbox_resp.right_hit)
                s3_output_type = s3_send_mem_req_output_type;
            else
                s3_output_type = s3_stack_op_output_type;
        end
        `IST_MEM_RESP: s3_output_type = `IST_REQ;
        `IST_RESP: begin
            if (s3_local_mem_n.num_trigs_left == 0)
                s3_output_type = s3_stack_op_output_type;
            else
                s3_output_type = `IST_REQ;
        end
    endcase
end

endmodule
