`ifndef AQB48_DATATYPES_H
`define AQB48_DATATYPES_H

`include "../include/params.vh"

typedef logic        bool;
typedef logic [ 4:0] ui5_t;
typedef logic [ 6:0] ui7_t;
typedef logic [ 7:0] ui8_t;
typedef logic [31:0] i32_t;
typedef logic [31:0] fp32_t;

typedef logic [        `RID_WIDTH-1:0] rid_t;
typedef logic [    `FIELD_B_WIDTH-1:0] field_b_t;
typedef logic [    `FIELD_C_WIDTH-1:0] field_c_t;
typedef logic [`CLUSTER_IDX_WIDTH-1:0] cluster_idx_t;
typedef logic [  `CHILD_IDX_WIDTH-1:0] child_idx_t;
typedef logic [ `STACK_SIZE_WIDTH-1:0] stack_size_t;

typedef struct packed {
    fp32_t origin_x;
    fp32_t origin_y;
    fp32_t origin_z;
    fp32_t dir_x;
    fp32_t dir_y;
    fp32_t dir_z;
    fp32_t tmin;
    fp32_t tmax;
} ray_t;

typedef struct packed {
    fp32_t w_x;
    fp32_t w_y;
    fp32_t w_z;
    fp32_t b_x;
    fp32_t b_y;
    fp32_t b_z;
    fp32_t tmin;
    fp32_t tmax;
} preprocessed_ray_t;

typedef struct packed {
    bool iw_x;
    bool iw_y;
    bool iw_z;
    ui7_t qw_l_x;
    ui7_t qw_l_y;
    ui7_t qw_l_z;
    ui7_t qw_h_x;
    ui7_t qw_h_y;
    ui7_t qw_h_z;
    ui5_t rw_l_x;
    ui5_t rw_l_y;
    ui5_t rw_l_z;
    ui5_t rw_h_x;
    ui5_t rw_h_y;
    ui5_t rw_h_z;
    i32_t qb_l_x;
    i32_t qb_l_y;
    i32_t qb_l_z;
    i32_t qy_max;
} quant_ray_t;

typedef struct packed {
    fp32_t origin_x;
    fp32_t origin_y;
    fp32_t origin_z;
    fp32_t dir_x;
    fp32_t dir_y;
    fp32_t dir_z;
    fp32_t w_x;
    fp32_t w_y;
    fp32_t w_z;
    fp32_t b_x;
    fp32_t b_y;
    fp32_t b_z;
    ui7_t qw_l_x;
    ui7_t qw_l_y;
    ui7_t qw_l_z;
    ui7_t qw_h_x;
    ui7_t qw_h_y;
    ui7_t qw_h_z;
    ui5_t rw_l_x;
    ui5_t rw_l_y;
    ui5_t rw_l_z;
    ui5_t rw_h_x;
    ui5_t rw_h_y;
    ui5_t rw_h_z;
    fp32_t tmin;
    fp32_t tmax;
} all_ray_t;

typedef struct packed {
    fp32_t p0_x;
    fp32_t p0_y;
    fp32_t p0_z;
    fp32_t e1_x;
    fp32_t e1_y;
    fp32_t e1_z;
    fp32_t e2_x;
    fp32_t e2_y;
    fp32_t e2_z;
} trig_t;

// ===========================================
// node_t format (assume FIELD_B_WIDTH == 3):
// INTERNAL: |1|0|0|0|-|-|-|-|-|-|-|-|-|-|-|-|
//     LEAF: |1|*|*|*|-|-|-|-|-|-|-|-|-|-|-|-|
//   SWITCH: |0|-|-|-|-|-|-|-|-|-|-|-|-|-|-|-|
//           \a/\ b /\           c           /
// for INTERNAL:
//   -: local_nbp_idx
// for LEAF:
//   *: num_trigs
//   -: local_trig_idx
// for SWITCH:
//   -: cluster_idx
// ===========================================
typedef struct packed {
    bool field_a;
    field_b_t field_b;
    field_c_t field_c;
} node_t;

typedef struct packed {
    ui8_t x_min;
    ui8_t x_max;
    ui8_t y_min;
    ui8_t y_max;
    ui8_t z_min;
    ui8_t z_max;
} bbox_t;

typedef struct packed {
    fp32_t x_min;
    fp32_t x_max;
    fp32_t y_min;
    fp32_t y_max;
    fp32_t z_min;
    fp32_t z_max;
    fp32_t inv_sx_inv_sw;
    child_idx_t nbp_offset;
    child_idx_t trig_offset;
} cluster_t;

typedef struct packed {
    node_t node;
    cluster_idx_t cluster_idx;
} node_stack_data_t;

// nbp: node-bbox pair
typedef struct packed {
    node_t left_node;
    node_t right_node;
    bbox_t left_bbox;
    bbox_t right_bbox;
} nbp_t;

typedef struct packed {
    bool intersected;
    fp32_t t;
    fp32_t u;
    fp32_t v;
} result_t;

typedef struct packed {
    rid_t rid;
    ray_t ray;
} init_req_t;

typedef struct packed {
    rid_t rid;
    all_ray_t all_ray;
} trv_req_t;

typedef struct packed {
    rid_t rid;
    result_t result;
} trv_resp_t;

typedef struct packed {
    rid_t rid;
    cluster_idx_t cluster_idx;
} clstr_mem_req_t;

typedef struct packed {
    rid_t rid;
    cluster_t cluster;
} clstr_mem_resp_t;

typedef struct packed {
    rid_t rid;
    preprocessed_ray_t preprocessed_ray;
    fp32_t inv_sx_inv_sw;
    fp32_t x_min;
    fp32_t x_max;
    fp32_t y_min;
    fp32_t y_max;
    fp32_t z_min;
    fp32_t z_max;
} clstr_req_t;

typedef struct packed {
    rid_t rid;
    bool intersected;
    fp32_t inv_sx_inv_sw;
    fp32_t y_ref;
    i32_t qb_l_x;
    i32_t qb_l_y;
    i32_t qb_l_z;
    i32_t qy_max;
} clstr_resp_t;

typedef struct packed {
    rid_t rid;
    fp32_t inv_sx_inv_sw;
    fp32_t y_ref;
    fp32_t tmax;
} updt_req_t;

typedef struct packed {
    rid_t rid;
    i32_t qy_max;
} updt_resp_t;

typedef struct packed {
    rid_t rid;
    child_idx_t nbp_idx;
} bbox_mem_req_t;

typedef struct packed {
    rid_t rid;
    nbp_t nbp;
} bbox_mem_resp_t;

typedef struct packed {
    rid_t rid;
    quant_ray_t quant_ray;
    bbox_t left_bbox;
    bbox_t right_bbox;
} bbox_req_t;

typedef struct packed {
    rid_t rid;
    bool left_hit;
    bool right_hit;
    bool left_first;
} bbox_resp_t;

typedef struct packed {
    rid_t rid;
    field_b_t num_trigs;
    child_idx_t trig_idx;
} ist_mem_req_t;

typedef struct packed {
    rid_t rid;
} ist_mem_resp_t;

typedef struct packed {
    rid_t rid;
    ray_t ray;
    trig_t trig;
} ist_req_t;

typedef struct packed {
    rid_t rid;
    bool intersected;
    fp32_t t;
    fp32_t u;
    fp32_t v;
} ist_resp_t;

typedef struct packed {
    stack_size_t node_stack_size;
} local_mem_a_t;

typedef struct packed {
    bool reintersect;
} local_mem_b_t;

typedef struct packed {
    cluster_idx_t cluster_idx;
} local_mem_c_t;

typedef struct packed {
    child_idx_t nbp_offset;
} local_mem_d_t;

typedef struct packed {
    child_idx_t trig_offset;
} local_mem_e_t;

typedef struct packed {
    i32_t qb_l_x;
    i32_t qb_l_y;
    i32_t qb_l_z;
} local_mem_f_t;

typedef struct packed {
    fp32_t origin_x;
    fp32_t origin_y;
    fp32_t origin_z;
    fp32_t dir_x;
    fp32_t dir_y;
    fp32_t dir_z;
} local_mem_g_t;

typedef struct packed {
    fp32_t w_x;
    fp32_t w_y;
    fp32_t w_z;
    fp32_t b_x;
    fp32_t b_y;
    fp32_t b_z;
} local_mem_h_t;

typedef struct packed {
    bool iw_x;
    bool iw_y;
    bool iw_z;
    ui7_t qw_l_x;
    ui7_t qw_l_y;
    ui7_t qw_l_z;
    ui7_t qw_h_x;
    ui7_t qw_h_y;
    ui7_t qw_h_z;
    ui5_t rw_l_x;
    ui5_t rw_l_y;
    ui5_t rw_l_z;
    ui5_t rw_h_x;
    ui5_t rw_h_y;
    ui5_t rw_h_z;
} local_mem_i_t;

typedef struct packed {
    fp32_t tmin;
} local_mem_j_t;

typedef struct packed {
    fp32_t tmax;
} local_mem_k_t;

typedef struct packed {
    bool intersected;
    fp32_t u;
    fp32_t v;
} local_mem_l_t;

typedef struct packed {
    node_t left_node;
    node_t right_node;
} local_mem_m_t;

typedef struct packed {
    field_b_t num_trigs_left;
} local_mem_n_t;

typedef struct packed {
    bool recompute_qymax;
} local_mem_o_t;

typedef struct packed {
    fp32_t inv_sx_inv_sw;
    fp32_t y_ref;
} local_mem_p_t;

typedef struct packed {
    i32_t qy_max;
} local_mem_q_t;

`define RAY_WIDTH              $bits(ray_t)
`define PREPROCESSED_RAY_WIDTH $bits(preprocessed_ray_t)
`define QUANT_RAY_WIDTH        $bits(quant_ray_t)
`define ALL_RAY_WIDTH          $bits(all_ray_t)
`define TRIG_WIDTH             $bits(trig_t)
`define NODE_WIDTH             $bits(node_t)
`define BBOX_WIDTH             $bits(bbox_t)
`define CLUSTER_WIDTH          $bits(cluster_t)
`define NODE_STACK_DATA_WIDTH  $bits(node_stack_data_t)
`define NBP_WIDTH              $bits(nbp_t)
`define RESULT_WIDTH           $bits(result_t)
`define INIT_REQ_WIDTH         $bits(init_req_t)
`define TRV_REQ_WIDTH          $bits(trv_req_t)
`define TRV_RESP_WIDTH         $bits(trv_resp_t)
`define CLSTR_MEM_REQ_WIDTH    $bits(clstr_mem_req_t)
`define CLSTR_MEM_RESP_WIDTH   $bits(clstr_mem_resp_t)
`define CLSTR_REQ_WIDTH        $bits(clstr_req_t)
`define CLSTR_RESP_WIDTH       $bits(clstr_resp_t)
`define UPDT_REQ_WIDTH         $bits(updt_req_t)
`define UPDT_RESP_WIDTH        $bits(updt_resp_t)
`define BBOX_MEM_REQ_WIDTH     $bits(bbox_mem_req_t)
`define BBOX_MEM_RESP_WIDTH    $bits(bbox_mem_resp_t)
`define BBOX_REQ_WIDTH         $bits(bbox_req_t)
`define BBOX_RESP_WIDTH        $bits(bbox_resp_t)
`define IST_MEM_REQ_WIDTH      $bits(ist_mem_req_t)
`define IST_MEM_RESP_WIDTH     $bits(ist_mem_resp_t)
`define IST_REQ_WIDTH          $bits(ist_req_t)
`define IST_RESP_WIDTH         $bits(ist_resp_t)
`define LOCAL_MEM_A_WIDTH      $bits(local_mem_a_t)
`define LOCAL_MEM_B_WIDTH      $bits(local_mem_b_t)
`define LOCAL_MEM_C_WIDTH      $bits(local_mem_c_t)
`define LOCAL_MEM_D_WIDTH      $bits(local_mem_d_t)
`define LOCAL_MEM_E_WIDTH      $bits(local_mem_e_t)
`define LOCAL_MEM_F_WIDTH      $bits(local_mem_f_t)
`define LOCAL_MEM_G_WIDTH      $bits(local_mem_g_t)
`define LOCAL_MEM_H_WIDTH      $bits(local_mem_h_t)
`define LOCAL_MEM_I_WIDTH      $bits(local_mem_i_t)
`define LOCAL_MEM_J_WIDTH      $bits(local_mem_j_t)
`define LOCAL_MEM_K_WIDTH      $bits(local_mem_k_t)
`define LOCAL_MEM_L_WIDTH      $bits(local_mem_l_t)
`define LOCAL_MEM_M_WIDTH      $bits(local_mem_m_t)
`define LOCAL_MEM_N_WIDTH      $bits(local_mem_n_t)
`define LOCAL_MEM_O_WIDTH      $bits(local_mem_o_t)
`define LOCAL_MEM_P_WIDTH      $bits(local_mem_p_t)
`define LOCAL_MEM_Q_WIDTH      $bits(local_mem_q_t)

`endif  // AQB48_DATATYPES_H
