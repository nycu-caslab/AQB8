`ifndef AQB48_DATATYPES_H
`define AQB48_DATATYPES_H

`include "../include/params.vh"

typedef logic        bool;
typedef logic [ 7:0] ui8_t;
typedef logic [31:0] fp32_t;

typedef logic [       `RID_WIDTH-1:0] rid_t;
typedef logic [ `NUM_TRIGS_WIDTH-1:0] num_trigs_t;
typedef logic [ `CHILD_IDX_WIDTH-1:0] child_idx_t;
typedef logic [`STACK_SIZE_WIDTH-1:0] stack_size_t;

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

typedef struct packed {
    num_trigs_t num_trigs;
    child_idx_t child_idx;
} node_t;

typedef struct packed {
    fp32_t x_min;
    fp32_t y_min;
    fp32_t z_min;
} ref_bbox_t;

typedef struct packed {
    ui8_t x_min;
    ui8_t x_max;
    ui8_t y_min;
    ui8_t y_max;
    ui8_t z_min;
    ui8_t z_max;
} bbox_t;

typedef struct packed {
    node_t node;
    ref_bbox_t b;
} node_stack_data_t;

typedef struct packed {
    ui8_t x;
    ui8_t y;
    ui8_t z;
} exp_t;

// nbp: node-bbox pair
typedef struct packed {
    node_t left_node;
    node_t right_node;
    exp_t exp;
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
    ref_bbox_t ref_bbox;
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
    child_idx_t nbp_idx;
} bbox_mem_req_t;

typedef struct packed {
    rid_t rid;
    nbp_t nbp;
} bbox_mem_resp_t;

typedef struct packed {
    rid_t rid;
    preprocessed_ray_t preprocessed_ray;
    exp_t exp;
    bbox_t left_bbox;
    bbox_t right_bbox;
} bbox_req_t;

typedef struct packed {
    rid_t rid;
    ref_bbox_t left_b;
    ref_bbox_t right_b;
    bool left_hit;
    bool right_hit;
    bool left_first;
} bbox_resp_t;

typedef struct packed {
    rid_t rid;
    num_trigs_t num_trigs;
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
    fp32_t origin_x;
    fp32_t origin_y;
    fp32_t origin_z;
    fp32_t dir_x;
    fp32_t dir_y;
    fp32_t dir_z;
} local_mem_b_t;

typedef struct packed {
    fp32_t w_x;
    fp32_t w_y;
    fp32_t w_z;
} local_mem_c_t;

typedef struct packed {
    fp32_t tmin;
} local_mem_d_t;

typedef struct packed {
    fp32_t tmax;
} local_mem_e_t;

typedef struct packed {
    bool intersected;
    fp32_t u;
    fp32_t v;
} local_mem_f_t;

typedef struct packed {
    node_t left_node;
    node_t right_node;
} local_mem_g_t;

typedef struct packed {
    num_trigs_t num_trigs_left;
} local_mem_h_t;

typedef struct packed {
    ref_bbox_t b;
} local_mem_i_t;

`define RAY_WIDTH              $bits(ray_t)
`define PREPROCESSED_RAY_WIDTH $bits(preprocessed_ray_t)
`define ALL_RAY_WIDTH          $bits(all_ray_t)
`define TRIG_WIDTH             $bits(trig_t)
`define NODE_WIDTH             $bits(node_t)
`define REF_BBOX_WIDTH         $bits(ref_bbox_t)
`define BBOX_WIDTH             $bits(bbox_t)
`define NODE_STACK_DATA_WIDTH  $bits(node_stack_data_t)
`define EXP_WIDTH              $bits(exp_t)
`define NBP_WIDTH              $bits(nbp_t)
`define RESULT_WIDTH           $bits(result_t)
`define INIT_REQ_WIDTH         $bits(init_req_t)
`define TRV_REQ_WIDTH          $bits(trv_req_t)
`define TRV_RESP_WIDTH         $bits(trv_resp_t)
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

`endif  // AQB48_DATATYPES_H
