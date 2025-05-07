#ifndef AQB48_GENTYPES_H
#define AQB48_GENTYPES_H

#include <cstdint>

struct gen_nbp_t {
    uint32_t left_node_num_trigs;
    uint32_t left_node_child_idx;
    uint32_t right_node_num_trigs;
    uint32_t right_node_child_idx;
    float left_bbox_x_min;
    float left_bbox_x_max;
    float left_bbox_y_min;
    float left_bbox_y_max;
    float left_bbox_z_min;
    float left_bbox_z_max;
    float right_bbox_x_min;
    float right_bbox_x_max;
    float right_bbox_y_min;
    float right_bbox_y_max;
    float right_bbox_z_min;
    float right_bbox_z_max;
};

#endif  // AQB48_GENTYPES_H