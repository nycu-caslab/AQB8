#ifndef BUILD_HPP
#define BUILD_HPP

#include <iostream>
#include <fstream>
#include <vector>
#include <bvh/triangle.hpp>
#include <bvh/sweep_sah_builder.hpp>
#include <array>
#include <cassert>
#include <queue>
#include <stack>
#include <memory>
#include <string>
#include <tuple>

#define INT_BVH_ALIGNMENT 64
#define INT_BVH_CLUSTER_length 36
#define INT_BVH_TRIG_length 36
#define INT_BVH_NODE_length 16
#define INT_BVH_PRIMITIVE_INSTANCE_length 8

namespace bvh_quantize
{
    // int_node_t::data format (assume field_b_bits = 3):
    // INTERNAL: |1|0|0|0|-|-|-|-|-|-|-|-|-|-|-|-|
    //     LEAF: |1|*|*|*|-|-|-|-|-|-|-|-|-|-|-|-|
    //   SWITCH: |0|-|-|-|-|-|-|-|-|-|-|-|-|-|-|-|
    //           \a/\ b /\           c           /
    // for INTERNAL:
    //   -: left_node_idx
    // for LEAF:
    //   *: num_trigs
    //   -: trig_idx
    // for SWITCH:
    //   -: child_cluster_idx

    constexpr int field_b_bits = 3;
    constexpr int field_c_bits = 15 - field_b_bits;
    constexpr int max_node_in_cluster_size = (1 << field_c_bits);
    constexpr size_t max_trig_in_leaf_size = (1 << field_b_bits) - 1;
    constexpr int max_trig_in_cluster_size = max_node_in_cluster_size;
    constexpr int max_cluster_size = (1 << 15);
    constexpr auto inv_sw = static_cast<float>(1 << 7);
    constexpr int qx_max = (1 << 8) - 1;

    typedef bvh::Bvh<float> bvh_t;
    typedef bvh::Triangle<float> trig_t;
    typedef bvh::Vector3<float> vector_t;
    typedef bvh::BoundingBox<float> bbox_t;
    typedef bvh::SweepSahBuilder<bvh_t> builder_t;
    typedef bvh_t::Node node_t;

    struct arg_t
    {
        char *model_file;
        float t_trv_int;
        float t_switch;
        float t_ist;
        char *ray_file;
    };

    enum class policy_t
    {
        STAY,
        SWITCH
    };

    enum class child_type_t
    {
        INTERNAL,
        LEAF,
        SWITCH
    };

    struct triangle_t
    {
        float v[3][3];
    };

    struct int_node_t
    {
        uint8_t bounds[6];
        uint16_t data;
    };

    struct int_node_v2_t
    {
        uint8_t left_bounds[6];
        uint16_t left_child_data;
        uint8_t right_bounds[6];
        uint16_t right_child_data;
    };

    struct int_cluster_t
    {
        float ref_bounds[6];
        float inv_sx_inv_sw;
        uint32_t node_offset;
        uint32_t trig_offset;
    };

    struct int_bvh_t
    {
        int num_clusters = 0;
        std::unique_ptr<int_cluster_t[]> clusters;
        std::unique_ptr<trig_t[]> trigs;
        std::unique_ptr<int_node_t[]> nodes;
        std::unique_ptr<int_node_v2_t[]> nodes_v2;
        std::unique_ptr<size_t[]> primitive_indices;
    };

    struct int_bvh_v2_t
    {
        int num_clusters = 0;
        std::unique_ptr<int_cluster_t[]> clusters;
        std::unique_ptr<trig_t[]> trigs;
        std::unique_ptr<int_node_v2_t[]> nodes_v2;
        std::unique_ptr<size_t[]> primitive_indices;
    };

    struct decoded_data_t
    {
        child_type_t child_type;
        uint8_t num_trigs;
        uint16_t idx;
    };

    // Function declarations
    int32_t floor_to_int32(float x);
    int32_t ceil_to_int32(float x);
    float get_scaling_factor(const bvh_t &bvh, size_t ref_idx);
    std::array<uint8_t, 6> get_int_bounds(const bvh_t &bvh, size_t node_idx, size_t ref_idx, float scaling_factor);
    bbox_t get_quant_bbox(const bvh_t &bvh, size_t node_idx, size_t ref_idx, float scaling_factor);
    arg_t parse_arg(int argc, char *argv[]);
    bvh_t build_bvh(const std::vector<trig_t> &trigs);
    std::vector<policy_t> get_policy(float t_trv_int, float t_switch, float t_ist, const bvh_t &bvh);
    int_bvh_t build_int_bvh(float t_trv_int, float t_switch, float t_ist, const std::vector<trig_t> &trigs, const bvh_t &bvh);
    decoded_data_t decode_data(uint16_t data);
    int_bvh_v2_t convert_nodes(const int_bvh_t &int_bvh, const bvh_t &bvh, const std::vector<trig_t> &trigs);

} // namespace bvh_quantize

#endif // BUILD_HPP
