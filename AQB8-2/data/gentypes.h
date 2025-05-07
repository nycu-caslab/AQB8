#ifndef AQB48_GENTYPES_H
#define AQB48_GENTYPES_H

#include <cstdint>

struct gen_nbp_t {
    uint32_t left_node_field_a;
    uint32_t left_node_field_b;
    uint32_t left_node_field_c;
    uint32_t right_node_field_a;
    uint32_t right_node_field_b;
    uint32_t right_node_field_c;
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

struct gen_cluster_t {
    float x_min;
    float x_max;
    float y_min;
    float y_max;
    float z_min;
    float z_max;
    float inv_sx_inv_sw;
    uint32_t nbp_offset;
    uint32_t trig_offset;
};

#endif  // AQB48_GENTYPES_H