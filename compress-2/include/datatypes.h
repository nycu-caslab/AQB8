#ifndef AQB48_DATATYPES_H
#define AQB48_DATATYPES_H

#include <ac_std_float.h>
#include <ac_int.h>
#include "params.h"

#define FP_MUL   lp_piped_fp_mult<AC_RND_CONV, 0>
#define FP_ADD   lp_piped_fp_add<AC_RND_CONV, 0>
#define FP_RECIP lp_piped_fp_recip<AC_RND_CONV, 0>

typedef ac_int<8, false> ui8_t;
typedef ac_std_float<32, 8> fp32_t;

typedef ac_int<RID_WIDTH, false> rid_t;
typedef ac_int<NUM_TRIGS_WIDTH, false> num_trigs_t;
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

typedef struct {
    num_trigs_t num_trigs;
    child_idx_t child_idx;
} node_t;

typedef struct {
    fp32_t x_min;
    fp32_t y_min;
    fp32_t z_min;
} ref_bbox_t;

typedef struct {
    ui8_t x_min;
    ui8_t x_max;
    ui8_t y_min;
    ui8_t y_max;
    ui8_t z_min;
    ui8_t z_max;
} bbox_t;

typedef struct {
    node_t node;
    ref_bbox_t b;
} node_stack_data_t;

typedef struct {
    ui8_t x;
    ui8_t y;
    ui8_t z;
} exp_t;

// nbp: node-bbox pair
typedef struct {
    node_t left_node;
    node_t right_node;
    exp_t exp;
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
    ref_bbox_t ref_bbox;
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
    child_idx_t nbp_idx;
} bbox_mem_req_t;

typedef struct {
    rid_t rid;
    nbp_t nbp;
} bbox_mem_resp_t;

typedef struct {
    rid_t rid;
    preprocessed_ray_t preprocessed_ray;
    exp_t exp;
    bbox_t left_bbox;
    bbox_t right_bbox;
} bbox_req_t;

typedef struct {
    rid_t rid;
    ref_bbox_t left_b;
    ref_bbox_t right_b;
    bool left_hit;
    bool right_hit;
    bool left_first;
} bbox_resp_t;

typedef struct {
    rid_t rid;
    num_trigs_t num_trigs;
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
