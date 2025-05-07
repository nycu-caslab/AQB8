#ifndef AQB48_GENTYPES_H
#define AQB48_GENTYPES_H

#include <cstdint>

struct gen_nbp_t {
    uint32_t left_node_num_trigs;
    uint32_t left_node_child_idx;
    uint32_t right_node_num_trigs;
    uint32_t right_node_child_idx;
    uint8_t exp_x;
    uint8_t exp_y;
    uint8_t exp_z;
    uint8_t left_bbox_x_min;
    uint8_t left_bbox_x_max;
    uint8_t left_bbox_y_min;
    uint8_t left_bbox_y_max;
    uint8_t left_bbox_z_min;
    uint8_t left_bbox_z_max;
    uint8_t right_bbox_x_min;
    uint8_t right_bbox_x_max;
    uint8_t right_bbox_y_min;
    uint8_t right_bbox_y_max;
    uint8_t right_bbox_z_min;
    uint8_t right_bbox_z_max;
};

struct gen_ref_bbox_t {
    float x_min;
    float y_min;
    float z_min;
};

#endif  // AQB48_GENTYPES_H