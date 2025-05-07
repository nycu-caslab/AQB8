#ifndef INT_BVH_HPP
#define INT_BVH_HPP

#include <iostream>
#include <memory>

#define INT_BVH_ALIGNMENT 64
#define INT_BVH_CLUSTER_length 36
#define INT_BVH_TRIG_length 36
#define INT_BVH_NODE_length 16
#define INT_BVH_PRIMITIVE_INSTANCE_length 4

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

    // Constants
    constexpr int field_b_bits = 3;
    constexpr int field_c_bits = 15 - field_b_bits;
    constexpr int max_node_in_cluster_size = (1 << field_c_bits);
    constexpr size_t max_trig_in_leaf_size = (1 << field_b_bits) - 1;
    // constexpr size_t max_trig_in_leaf_size = 6;
    constexpr int max_trig_in_cluster_size = max_node_in_cluster_size;
    constexpr int max_cluster_size = (1 << 15);

    constexpr auto inv_sw = static_cast<float>(1 << 7);
    constexpr int qx_max = (1 << 8) - 1;

    // Type Definitions
    enum class child_type_t
    {
        INTERNAL,
        LEAF,
        SWITCH
    };

    struct int_trig_t
    {
        float v[3][3];
    };

    struct int_node_t
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
        std::unique_ptr<int_trig_t[]> trigs;
        std::unique_ptr<int_node_t[]> nodes;
        std::unique_ptr<size_t[]> primitive_indices;
    };

    struct decoded_data_t
    {
        child_type_t child_type;
        uint8_t num_trigs;
        uint16_t idx;
    };

} // namespace bvh_quantize

#endif // INT_BVH_HPP
