#ifndef AQB48_DATATYPES_H
#define AQB48_DATATYPES_H

#include <ac_std_float.h>
#include <ac_int.h>
#include "params.h"

#define FP_MUL   lp_piped_fp_mult<AC_RND_CONV, 0>
#define FP_ADD   lp_piped_fp_add<AC_RND_CONV, 0>
#define FP_RECIP lp_piped_fp_recip<AC_RND_CONV, 0>
#define FP_FLT2I fp_flt2i<AC_RND_CONV, 0, 32>

typedef ac_int<5, false> ui5_t;
typedef ac_int<7, false> ui7_t;
typedef ac_int<8, false> ui8_t;
typedef ac_int<32, true> i32_t;
typedef ac_std_float<32, 8> fp32_t;

typedef ac_int<RID_WIDTH, false> rid_t;
typedef ac_int<FIELD_B_WIDTH, false> field_b_t;
typedef ac_int<FIELD_C_WIDTH, false> field_c_t;
typedef ac_int<CLUSTER_IDX_WIDTH, false> cluster_idx_t;
typedef ac_int<CHILD_IDX_WIDTH, false> child_idx_t;
typedef ac_int<STACK_SIZE_WIDTH, false> stack_size_t;

typedef struct {
    fp32_t origin_x;
    fp32_t origin_y;
    fp32_t origin_z;
    fp32_t dir_x;
    fp32_t dir_y;
    fp32_t dir_z;
    fp32_t tmin;
    fp32_t tmax;
} ray_t;

typedef struct {
    fp32_t w_x;
    fp32_t w_y;
    fp32_t w_z;
    fp32_t b_x;
    fp32_t b_y;
    fp32_t b_z;
    fp32_t tmin;
    fp32_t tmax;
} preprocessed_ray_t;

typedef struct {
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

typedef struct {
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

typedef struct {
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
typedef struct {
    bool field_a;
    field_b_t field_b;
    field_c_t field_c;
} node_t;

typedef struct {
    ui8_t x_min;
    ui8_t x_max;
    ui8_t y_min;
    ui8_t y_max;
    ui8_t z_min;
    ui8_t z_max;
} bbox_t;

typedef struct {
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

typedef struct {
    node_t node;
    cluster_idx_t cluster_idx;
} node_stack_data_t;

// nbp: node-bbox pair
typedef struct {
    node_t left_node;
    node_t right_node;
    bbox_t left_bbox;
    bbox_t right_bbox;
} nbp_t;

typedef struct {
    bool intersected;
    fp32_t t;
    fp32_t u;
    fp32_t v;
} result_t;

typedef struct {
    rid_t rid;
    ray_t ray;
} init_req_t;

typedef struct {
    rid_t rid;
    all_ray_t all_ray;
} trv_req_t;

typedef struct {
    rid_t rid;
    result_t result;
} trv_resp_t;

typedef struct {
    rid_t rid;
    cluster_idx_t cluster_idx;
} clstr_mem_req_t;

typedef struct {
    rid_t rid;
    cluster_t cluster;
} clstr_mem_resp_t;

typedef struct {
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

typedef struct {
    rid_t rid;
    bool intersected;
    fp32_t inv_sx_inv_sw;
    fp32_t y_ref;
    i32_t qb_l_x;
    i32_t qb_l_y;
    i32_t qb_l_z;
    i32_t qy_max;
} clstr_resp_t;

typedef struct {
    rid_t rid;
    fp32_t inv_sx_inv_sw;
    fp32_t y_ref;
    fp32_t tmax;
} updt_req_t;

typedef struct {
    rid_t rid;
    i32_t qy_max;
} updt_resp_t;

typedef struct {
    rid_t rid;
    child_idx_t nbp_idx;
} bbox_mem_req_t;

typedef struct {
    rid_t rid;
    nbp_t nbp;
} bbox_mem_resp_t;

typedef struct {
    rid_t rid;
    quant_ray_t quant_ray;
    bbox_t left_bbox;
    bbox_t right_bbox;
} bbox_req_t;

typedef struct {
    rid_t rid;
    bool left_hit;
    bool right_hit;
    bool left_first;
} bbox_resp_t;

typedef struct {
    rid_t rid;
    field_b_t num_trigs;
    child_idx_t trig_idx;
} ist_mem_req_t;

typedef struct {
    rid_t rid;
} ist_mem_resp_t;

typedef struct {
    rid_t rid;
    ray_t ray;
    trig_t trig;
} ist_req_t;

typedef struct {
    rid_t rid;
    bool intersected;
    fp32_t t;
    fp32_t u;
    fp32_t v;
} ist_resp_t;

#endif  // AQB48_DATATYPES_H
