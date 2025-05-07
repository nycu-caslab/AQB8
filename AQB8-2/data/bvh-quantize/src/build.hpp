#ifndef BUILD_HPP
#define BUILD_HPP

#include <iostream>
#include <vector>
#include <bvh/triangle.hpp>
#include <bvh/sweep_sah_builder.hpp>
#include "happly/happly.h"

namespace bvh_quantize {

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

struct arg_t {
    char* model_file;
    float t_trv_int;
    float t_switch;
    float t_ist;
    char* ray_file;
};

enum class policy_t {
    STAY, SWITCH
};

enum class child_type_t {
    INTERNAL,  // children are in the same cluster and are internal nodes
    LEAF,      // children are in the same cluster and are leaf nodes
    SWITCH     // children are in different cluster
};

struct int_node_t {
    uint8_t bounds[6];
    uint16_t data;
};

struct int_cluster_t {
    float ref_bounds[6];
    float inv_sx_inv_sw;
    uint32_t node_offset;
    uint32_t trig_offset;
};

struct int_bvh_t {
    // clusters[0] is the top cluster
    int num_clusters = 0;
    std::unique_ptr<int_cluster_t[]> clusters;
    std::unique_ptr<trig_t[]> trigs;
    std::unique_ptr<int_node_t[]> nodes;
};

struct decoded_data_t {
    child_type_t child_type;
    uint8_t num_trigs;
    uint16_t idx;
};

int32_t floor_to_int32(float x) {
    assert(!std::isnan(x));
    if (x < -2147483648.0f)
        return -2147483648;
    if (x >= 2147483648.0f)
        return 2147483647;
    return (int)floorf(x);
}

int32_t ceil_to_int32(float x) {
    assert(!std::isnan(x));
    if (x <= -2147483649.0f)
        return -2147483648;
    if (x > 2147483647.0f)
        return 2147483647;
    return (int)ceilf(x);
}

float get_scaling_factor(const bvh_t &bvh, size_t ref_idx) {
    bbox_t ref_bbox = bvh.nodes[ref_idx].bounding_box_proxy().to_bounding_box();

    float max_len = 0.0f;
    for (int i = 0; i < 3; i++)
        max_len = std::max(max_len, ref_bbox.max[i] - ref_bbox.min[i]);

    float scaling_factor = max_len / (float)qx_max;
    assert(scaling_factor > 0.0f);
    return scaling_factor;
}

// return: [qxmin, qxmax, qymin, qymax, qzmin, qzmax]
std::array<uint8_t, 6> get_int_bounds(const bvh_t &bvh, size_t node_idx, size_t ref_idx, float scaling_factor) {
    bbox_t node_bbox = bvh.nodes[node_idx].bounding_box_proxy().to_bounding_box();
    bbox_t ref_bbox = bvh.nodes[ref_idx].bounding_box_proxy().to_bounding_box();

    std::array<uint8_t, 6> ret{};
    for (int i = 0; i < 3; i++) {
        int min = floor_to_int32((node_bbox.min[i] - ref_bbox.min[i]) / scaling_factor);
        int max = ceil_to_int32((node_bbox.max[i] - ref_bbox.min[i]) / scaling_factor);
        assert(0 <= min && min <= qx_max + 1);
        assert(0 <= max && max <= qx_max + 1);
        if (min == qx_max + 1)
            min = qx_max;
        if (max == qx_max + 1)
            max = qx_max;
        ret[i * 2] = min;
        ret[i * 2 + 1] = max;
    }

    return ret;
}

bbox_t get_quant_bbox(const bvh_t &bvh, size_t node_idx, size_t ref_idx, float scaling_factor) {
    bbox_t ref_bbox = bvh.nodes[ref_idx].bounding_box_proxy().to_bounding_box();
    std::array<uint8_t, 6> int_bounds = get_int_bounds(bvh, node_idx, ref_idx, scaling_factor);

    bbox_t ret;
    for (int i = 0; i < 3; i++) {
        ret.min[i] = ref_bbox.min[i] + scaling_factor * (float)int_bounds[i * 2];
        ret.max[i] = ref_bbox.min[i] + scaling_factor * (float)int_bounds[i * 2 + 1];
        assert(std::isfinite(ret.min[i]));
        assert(std::isfinite(ret.max[i]));
    }
    return ret;
}

arg_t parse_arg(int argc, char *argv[]) {
    if (argc < 5) {
        std::cerr << "usage: " << argv[0] <<
                  " MODEL_FILE T_TRV_INT T_SWITCH T_IST [RAY_FILE]" << std::endl;
        exit(EXIT_FAILURE);
    }

    arg_t arg{};
    arg.model_file = argv[1];
    arg.t_trv_int = std::stof(argv[2]);
    arg.t_switch = std::stof(argv[3]);
    arg.t_ist = std::stof(argv[4]);
    std::cout << "MODEL_FILE = " << arg.model_file << std::endl;
    std::cout << "T_TRV_INT = " << arg.t_trv_int << std::endl;
    std::cout << "T_SWITCH = " << arg.t_switch << std::endl;
    std::cout << "T_IST = " << arg.t_ist << std::endl;

    arg.ray_file = nullptr;
    if (argc >= 6) {
        arg.ray_file = argv[5];
        std::cout << "RAY_FILE = " << arg.ray_file << std::endl;
    }

    return arg;
}

std::vector<trig_t> load_trigs(const char* model_file) {
    happly::PLYData ply_data(model_file);
    std::vector<std::array<double, 3>> v_pos = ply_data.getVertexPositions();
    std::vector<std::vector<size_t>> f_idx = ply_data.getFaceIndices<size_t>();

    std::vector<trig_t> trigs;
    for (auto &face : f_idx) {
        trigs.emplace_back(
            vector_t((float)v_pos[face[0]][0], (float)v_pos[face[0]][1], (float)v_pos[face[0]][2]),
            vector_t((float)v_pos[face[1]][0], (float)v_pos[face[1]][1], (float)v_pos[face[1]][2]),
            vector_t((float)v_pos[face[2]][0], (float)v_pos[face[2]][1], (float)v_pos[face[2]][2])
        );
    }
    return trigs;
}

bvh_t build_bvh(const std::vector<trig_t>& trigs) {
    auto [bboxes, centers] = bvh::compute_bounding_boxes_and_centers(trigs.data(), trigs.size());
    auto global_bbox = bvh::compute_bounding_boxes_union(bboxes.get(), trigs.size());
    std::cout << "global_bbox = ("
              << global_bbox.min[0] << ", " << global_bbox.min[1] << ", " << global_bbox.min[2] << "), ("
              << global_bbox.max[0] << ", " << global_bbox.max[1] << ", " << global_bbox.max[2] << ")" << std::endl;

    bvh_t bvh;
    builder_t builder(bvh);
    builder.max_leaf_size = max_trig_in_leaf_size;
    builder.build(global_bbox, bboxes.get(), centers.get(), trigs.size());

    return bvh;
}

std::vector<policy_t> get_policy(float t_trv_int, float t_switch, float t_ist, const bvh_t& bvh) {
    // stk_1: fill t_buf_size, t_buf_idx_map
    int t_buf_size = 0;
    std::vector<int> t_buf_idx_map(bvh.node_count);
    std::stack<std::pair<size_t, int>> stk_1;
    node_t& root_node = bvh.nodes[0];
    size_t root_left_node_idx = root_node.first_child_or_primitive;
    size_t root_right_node_idx = root_left_node_idx + 1;
    stk_1.emplace(root_right_node_idx, 1);
    stk_1.emplace(root_left_node_idx, 1);
    while (!stk_1.empty()) {
        auto [curr_node_idx, depth] = stk_1.top();
        node_t& curr_node = bvh.nodes[curr_node_idx];
        stk_1.pop();

        t_buf_idx_map[curr_node_idx] = t_buf_size;
        t_buf_size += depth;

        if (!curr_node.is_leaf()) {
            size_t left_node_idx = curr_node.first_child_or_primitive;
            size_t right_node_idx = left_node_idx + 1;
            stk_1.emplace(right_node_idx, depth + 1);
            stk_1.emplace(left_node_idx, depth + 1);
        }
    }

    // stk_2: fill parent, t_buf, t_policy
    std::vector<size_t> parent(bvh.node_count);
    parent[root_left_node_idx] = 0;
    parent[root_right_node_idx] = 0;
    std::vector<float> t_buf(t_buf_size);
    std::vector<policy_t> t_policy(t_buf_size);
    std::stack<std::pair<size_t, bool>> stk_2;
    stk_2.emplace(root_right_node_idx, true);
    stk_2.emplace(root_left_node_idx, true);
    while (!stk_2.empty()) {
        auto [curr_node_idx, first] = stk_2.top();
        node_t& curr_node = bvh.nodes[curr_node_idx];
        stk_2.pop();

        size_t left_node_idx = curr_node.first_child_or_primitive;
        size_t right_node_idx = left_node_idx + 1;

        if (first) {
            stk_2.emplace(curr_node_idx, false);
            if (!curr_node.is_leaf()) {
                stk_2.emplace(right_node_idx, true);
                stk_2.emplace(left_node_idx, true);
                parent[left_node_idx] = curr_node_idx;
                parent[right_node_idx] = curr_node_idx;
            }
        } else {
            size_t ref_idx = parent[curr_node_idx];
            for (int i = 0; ; i++) {
                float scaling_factor = get_scaling_factor(bvh, ref_idx);
                bbox_t quant_bbox = get_quant_bbox(bvh, curr_node_idx, ref_idx, scaling_factor);
                float half_area = quant_bbox.half_area();

                float& curr_t_buf = t_buf[t_buf_idx_map[curr_node_idx] + i];
                policy_t& curr_t_policy = t_policy[t_buf_idx_map[curr_node_idx] + i];
                if (curr_node.is_leaf()) {
                    curr_t_buf = t_ist * (float)curr_node.primitive_count * half_area;
                } else {
                    float left_stay_t = t_buf[t_buf_idx_map[left_node_idx] + 1 + i];
                    float right_stay_t = t_buf[t_buf_idx_map[right_node_idx] + 1 + i];

                    float left_switch_t = t_buf[t_buf_idx_map[left_node_idx]];
                    float right_switch_t = t_buf[t_buf_idx_map[right_node_idx]];

                    float curr_stay_t = t_trv_int * 2 * half_area + left_stay_t + right_stay_t;
                    float curr_switch_t = (t_trv_int * 2 + t_switch) * half_area + left_switch_t + right_switch_t;

                    assert(std::isfinite(curr_stay_t));
                    assert(std::isfinite(curr_switch_t));

                    if (curr_switch_t < curr_stay_t) {
                        curr_t_buf = curr_switch_t;
                        curr_t_policy = policy_t::SWITCH;
                    } else {
                        curr_t_buf = curr_stay_t;
                        curr_t_policy = policy_t::STAY;
                    }
                }

                if (ref_idx == 0)
                    break;
                else
                    ref_idx = parent[ref_idx];
            }
        }
    }

    // stk_3: fill policy
    std::vector<policy_t> policy(bvh.node_count);
    std::stack<std::pair<size_t, int>> stk_3;
    policy[0] = policy_t::SWITCH;
    stk_3.emplace(root_right_node_idx, 0);
    stk_3.emplace(root_left_node_idx, 0);
    while (!stk_3.empty()) {
        auto [curr_node_idx, curr_offset] = stk_3.top();
        node_t& curr_node = bvh.nodes[curr_node_idx];
        stk_3.pop();

        if (curr_node.is_leaf())
            continue;

        size_t left_node_idx = curr_node.first_child_or_primitive;
        size_t right_node_idx = left_node_idx + 1;

        switch (t_policy[t_buf_idx_map[curr_node_idx] + curr_offset]) {
            case policy_t::STAY:
                policy[curr_node_idx] = policy_t::STAY;
                stk_3.emplace(right_node_idx, 1 + curr_offset);
                stk_3.emplace(left_node_idx, 1 + curr_offset);
                break;
            case policy_t::SWITCH:
                policy[curr_node_idx] = policy_t::SWITCH;
                stk_3.emplace(right_node_idx, 0);
                stk_3.emplace(left_node_idx, 0);
                break;
        }
    }

    return policy;
}

int_bvh_t build_int_bvh(float t_trv_int, float t_switch, float t_ist, const std::vector<trig_t>& trigs,
                        const bvh_t& bvh) {
    // fill policy
    std::vector<policy_t> policy = get_policy(t_trv_int, t_switch, t_ist, bvh);

    // que: fill num_clusters, cluster_node_indices, ref_indices, local_node_idx_map
    int num_clusters = 0;
    std::vector<std::vector<size_t>> cluster_node_indices;
    std::vector<size_t> ref_indices;
    std::vector<size_t> local_node_idx_map(bvh.node_count);
    std::queue<std::pair<size_t, int>> que;
    que.emplace(0, -1);
    while (!que.empty()) {
        auto [curr_node_idx, curr_cluster_idx] = que.front();
        node_t& curr_node = bvh.nodes[curr_node_idx];
        que.pop();

        if (curr_node.is_leaf())
            continue;

        int child_cluster_idx = -1;
        switch (policy[curr_node_idx]) {
            case policy_t::STAY:
                child_cluster_idx = curr_cluster_idx;
                break;
            case policy_t::SWITCH:
                child_cluster_idx = num_clusters;
                num_clusters++;
                cluster_node_indices.emplace_back();
                ref_indices.push_back(curr_node_idx);
                break;
        }

        size_t left_node_idx = curr_node.first_child_or_primitive;
        size_t right_node_idx = left_node_idx + 1;
        local_node_idx_map[left_node_idx] = cluster_node_indices[child_cluster_idx].size();
        cluster_node_indices[child_cluster_idx].push_back(left_node_idx);
        local_node_idx_map[right_node_idx] = cluster_node_indices[child_cluster_idx].size();
        cluster_node_indices[child_cluster_idx].push_back(right_node_idx);

        que.emplace(left_node_idx, child_cluster_idx);
        que.emplace(right_node_idx, child_cluster_idx);
    }

    // fill cluster_idx_map, scaling_factors
    std::vector<int> cluster_idx_map(bvh.node_count);
    std::vector<float> scaling_factors(num_clusters);
    for (int i = 0; i < num_clusters; i++) {
        for (unsigned int j = 0; j < cluster_node_indices[i].size(); j++)
            cluster_idx_map[cluster_node_indices[i][j]] = i;
        scaling_factors[i] = get_scaling_factor(bvh, ref_indices[i]);
    }

    // fill int_bvh, local_trig_idx_map
    int_bvh_t int_bvh;
    int_bvh.num_clusters = num_clusters;
    int_bvh.clusters = std::make_unique<int_cluster_t[]>(num_clusters);
    int_bvh.trigs = std::make_unique<trig_t[]>(trigs.size());
    int_bvh.nodes = std::make_unique<int_node_t[]>(bvh.node_count);
    std::vector<size_t> local_trig_idx_map(bvh.node_count);
    size_t tmp_node_offset = 0;
    size_t tmp_trig_offset = 0;
    for (int i = 0; i < num_clusters; i++) {
        // fill int_bvh.clusters[i]
        for (int j = 0; j < 6; j++)
            int_bvh.clusters[i].ref_bounds[j] = bvh.nodes[ref_indices[i]].bounds[j];
        int_bvh.clusters[i].inv_sx_inv_sw = inv_sw / scaling_factors[i];
        int_bvh.clusters[i].node_offset = tmp_node_offset;
        int_bvh.clusters[i].trig_offset = tmp_trig_offset;

        // fill int_bvh.trigs
        int tmp_local_trig_offset = 0;
        for (size_t curr_node_idx : cluster_node_indices[i]) {
            node_t& curr_node = bvh.nodes[curr_node_idx];
            if (curr_node.is_leaf()) {
                local_trig_idx_map[curr_node_idx] = tmp_local_trig_offset;
                for (unsigned int j = 0; j < curr_node.primitive_count; j++) {
                    size_t trig_idx = bvh.primitive_indices[curr_node.first_child_or_primitive + j];
                    int_bvh.trigs[tmp_trig_offset] = trigs[trig_idx];
                    tmp_trig_offset++;
                    tmp_local_trig_offset++;
                }
            }
        }

        // fill int_bvh.nodes
        for (size_t curr_node_idx : cluster_node_indices[i]) {
            node_t& curr_node = bvh.nodes[curr_node_idx];
            int_node_t& curr_int_node = int_bvh.nodes[tmp_node_offset];
            tmp_node_offset++;

            // fill curr_int_node.bounds
            std::array<uint8_t, 6> bounds = get_int_bounds(bvh, curr_node_idx, ref_indices[i], scaling_factors[i]);
            for (int j = 0; j < 6; j++)
                curr_int_node.bounds[j] = bounds[j];

            child_type_t child_type;
            size_t left_node_idx = curr_node.first_child_or_primitive;
            size_t right_node_idx = left_node_idx + 1;
            if (curr_node.is_leaf()) {
                child_type = child_type_t::LEAF;
            } else {
                int curr_cluster_idx = cluster_idx_map[curr_node_idx];
                int left_cluster_idx = cluster_idx_map[left_node_idx];
                int right_cluster_idx = cluster_idx_map[right_node_idx];
                assert(left_cluster_idx == right_cluster_idx);

                if (curr_cluster_idx != left_cluster_idx) {
                    assert(policy[curr_node_idx] == policy_t::SWITCH);
                    child_type = child_type_t::SWITCH;
                } else {
                    assert(policy[curr_node_idx] == policy_t::STAY);
                    child_type = child_type_t::INTERNAL;
                }
            }

            // fill curr_int_node.data
            switch (child_type) {
                case child_type_t::INTERNAL: {
                    size_t field_c = local_node_idx_map[left_node_idx];
                    if (field_c >= max_node_in_cluster_size) {
                        std::cerr << "internal node cannot fit!" << std::endl;
                        exit(EXIT_FAILURE);
                    }
                    curr_int_node.data = 0x8000 | field_c;
                    break;
                }
                case child_type_t::LEAF: {
                    size_t field_b = bvh.nodes[curr_node_idx].primitive_count;
                    size_t field_c = local_trig_idx_map[curr_node_idx];
                    if (field_b > max_trig_in_leaf_size || field_c >= max_trig_in_cluster_size) {
                        std::cerr << "leaf node cannot fit!" << std::endl;
                        exit(EXIT_FAILURE);
                    }
                    curr_int_node.data = 0x8000 | (field_b << field_c_bits) | field_c;
                    break;
                }
                case child_type_t::SWITCH: {
                    size_t field_bc = cluster_idx_map[left_node_idx];
                    if (field_bc >= max_cluster_size) {
                        std::cerr << "switch node cannot fit!" << std::endl;
                        exit(EXIT_FAILURE);
                    }
                    curr_int_node.data = field_bc;
                    break;
                }
            }
        }
    }

    return int_bvh;
}

decoded_data_t decode_data(uint16_t data) {
    decoded_data_t decoded_data{};
    if (data & 0x8000) {
        int field_b = ((data & 0x7fff) >> field_c_bits);
        int field_c = (data & ((1 << field_c_bits) - 1));
        if (field_b == 0) {
            decoded_data.child_type = child_type_t::INTERNAL;
        } else {
            decoded_data.child_type = child_type_t::LEAF;
            decoded_data.num_trigs = field_b;
        }
        decoded_data.idx = field_c;
    } else {
        decoded_data.child_type = child_type_t::SWITCH;
        decoded_data.idx = data;
    }
    return decoded_data;
}

void gen_tree_visualization_and_report_stack_size_requirement(const int_bvh_t& int_bvh) {
    uint32_t root_left_node_idx = int_bvh.clusters[0].node_offset;
    uint32_t root_right_node_idx = root_left_node_idx + 1;
    std::array<std::string, 2> cmap = {"black", "red"};

    std::ofstream graph_fs("graph.dot");
    graph_fs << "digraph bvh {\n";
    graph_fs << "    layout=twopi\n";
    graph_fs << "    root=R\n";
    graph_fs << "    node [shape=point]\n";
    graph_fs << "    edge [arrowhead=none]\n";
    graph_fs << "    R [shape=circle label=root depth=0]\n";
    graph_fs << "    " << root_left_node_idx << " [depth=1]\n";
    graph_fs << "    " << root_right_node_idx << " [depth=1]\n";
    graph_fs << "    R -> " << root_left_node_idx << " [color=" << cmap[0] << "]\n";
    graph_fs << "    R -> " << root_right_node_idx << " [color=" << cmap[0] << "]\n";

    int node_stack_size_requirement = 0;
    int cluster_stack_size_requirement = 0;

    std::queue<std::tuple<int, int, int, int, int>> que;
    que.emplace(root_left_node_idx, 0, 0, 1, 0);
    que.emplace(root_right_node_idx, 0, 0, 1, 0);
    while (!que.empty()) {
        auto [curr_node_idx, curr_cluster_idx, curr_color, curr_depth, curr_cluster_depth] = que.front();
        int_node_t& curr_node = int_bvh.nodes[curr_node_idx];
        int_cluster_t& curr_cluster = int_bvh.clusters[curr_cluster_idx];
        que.pop();

        decoded_data_t decoded_data = decode_data(curr_node.data);

        int child_cluster_idx;
        uint32_t left_node_idx;
        uint32_t right_node_idx;
        int child_color;
        int child_depth = curr_depth + 1;
        int child_cluster_depth;
        switch (decoded_data.child_type) {
            case child_type_t::INTERNAL: {
                child_cluster_idx = curr_cluster_idx;
                left_node_idx = curr_cluster.node_offset + decoded_data.idx;
                right_node_idx = left_node_idx + 1;
                child_color = curr_color;
                child_cluster_depth = curr_cluster_depth;
                break;
            }
            case child_type_t::LEAF: {
                node_stack_size_requirement = std::max(node_stack_size_requirement, curr_depth);
                cluster_stack_size_requirement = std::max(cluster_stack_size_requirement, curr_cluster_depth);
                continue;  // this continue is related to the while loop
            }
            case child_type_t::SWITCH: {
                child_cluster_idx = decoded_data.idx;
                int_cluster_t& child_cluster = int_bvh.clusters[child_cluster_idx];
                left_node_idx = child_cluster.node_offset;
                right_node_idx = left_node_idx + 1;
                child_color = (curr_color + 1) % 2;
                child_cluster_depth = curr_cluster_depth + 1;
                break;
            }
        }

        que.emplace(left_node_idx, child_cluster_idx, child_color, child_depth, child_cluster_depth);
        que.emplace(right_node_idx, child_cluster_idx, child_color, child_depth, child_cluster_depth);
        graph_fs << "    " << left_node_idx << " [depth=" << child_depth << "]\n";
        graph_fs << "    " << right_node_idx << " [depth=" << child_depth << "]\n";
        graph_fs << "    " << curr_node_idx << " -> " << left_node_idx << " [color=" << cmap[child_color] << "]\n";
        graph_fs << "    " << curr_node_idx << " -> " << right_node_idx << " [color=" << cmap[child_color] << "]\n";
    }

    graph_fs << "}";

    std::cout << "node_stack_size_requirement = " << node_stack_size_requirement << std::endl;
    std::cout << "cluster_stack_size_requirement = " << cluster_stack_size_requirement << std::endl;
}

void gen_scene_visualization(const bvh_t& bvh, const int_bvh_t& int_bvh) {
    std::ofstream node_offsets_fs("node_offsets.bin", std::ios::binary);
    for (int i = 0; i < int_bvh.num_clusters; i++) {
        uint32_t offset = int_bvh.clusters[i].node_offset;
        node_offsets_fs.write((char*)(&offset), sizeof(offset));
    }

    // fill full_bboxes_by_cluster, quant_bboxes_by_cluster, trigs_by_cluster
    std::vector<std::vector<bbox_t>> full_bboxes_by_cluster(int_bvh.num_clusters);
    std::vector<std::vector<bbox_t>> quant_bboxes_by_cluster(int_bvh.num_clusters);
    std::vector<std::vector<trig_t>> trigs_by_cluster(int_bvh.num_clusters);
    std::queue<std::tuple<size_t, int, int>> que;
    size_t root_left_node_idx = bvh.nodes[0].first_child_or_primitive;
    size_t root_right_node_idx = root_left_node_idx + 1;
    uint32_t root_left_int_node_idx = int_bvh.clusters[0].node_offset;
    uint32_t root_right_int_node_idx = root_left_int_node_idx + 1;
    que.emplace(root_left_node_idx, root_left_int_node_idx, 0);
    que.emplace(root_right_node_idx, root_right_int_node_idx, 0);
    while (!que.empty()) {
        auto [curr_node_idx, curr_int_node_idx, curr_cluster_idx] = que.front();
        node_t& curr_node = bvh.nodes[curr_node_idx];
        int_node_t& curr_int_node = int_bvh.nodes[curr_int_node_idx];
        int_cluster_t& curr_cluster = int_bvh.clusters[curr_cluster_idx];
        que.pop();

        bbox_t full_bbox = curr_node.bounding_box_proxy().to_bounding_box();
        full_bboxes_by_cluster[curr_cluster_idx].push_back(full_bbox);

        bbox_t quant_bbox;
        for (int i = 0; i < 3; i++) {
            float scaling_factor = inv_sw / curr_cluster.inv_sx_inv_sw;
            quant_bbox.min[i] = curr_cluster.ref_bounds[i * 2] + scaling_factor * (float)curr_int_node.bounds[i * 2];
            quant_bbox.max[i] = curr_cluster.ref_bounds[i * 2] + scaling_factor * (float)curr_int_node.bounds[i * 2 + 1];
            assert(std::isfinite(quant_bbox.min[i]));
            assert(std::isfinite(quant_bbox.max[i]));
        }
        quant_bboxes_by_cluster[curr_cluster_idx].push_back(quant_bbox);

        decoded_data_t decoded_data = decode_data(curr_int_node.data);

        int child_cluster_idx;
        size_t left_node_idx = curr_node.first_child_or_primitive;
        size_t right_node_idx = left_node_idx + 1;
        uint32_t left_int_node_idx;
        uint32_t right_int_node_idx;
        switch (decoded_data.child_type) {
            case child_type_t::INTERNAL: {
                child_cluster_idx = curr_cluster_idx;
                left_int_node_idx = curr_cluster.node_offset + decoded_data.idx;
                right_int_node_idx = left_int_node_idx + 1;
                break;
            }
            case child_type_t::LEAF: {
                for (int i = 0; i < decoded_data.num_trigs; i++) {
                    trig_t trig = int_bvh.trigs[curr_cluster.trig_offset + decoded_data.idx + i];
                    trigs_by_cluster[curr_cluster_idx].push_back(trig);
                }
                continue;  // this continue is related to the while loop
            }
            case child_type_t::SWITCH: {
                child_cluster_idx = decoded_data.idx;
                int_cluster_t& child_cluster = int_bvh.clusters[child_cluster_idx];
                left_int_node_idx = child_cluster.node_offset;
                right_int_node_idx = left_int_node_idx + 1;
                break;
            }
        }

        que.emplace(left_node_idx, left_int_node_idx, child_cluster_idx);
        que.emplace(right_node_idx, right_int_node_idx, child_cluster_idx);
    }

    std::ofstream full_bboxes_fs("full_bboxes.bin", std::ios::binary);
    std::ofstream quant_bboxes_fs("quant_bboxes.bin", std::ios::binary);
    std::ofstream trigs_size_fs("trigs_size.bin", std::ios::binary);
    std::ofstream trigs_fs("trigs.bin", std::ios::binary);
    for (int i = 0; i < int_bvh.num_clusters; i++) {
        for (bbox_t& full_bbox : full_bboxes_by_cluster[i]) {
            full_bboxes_fs.write((char*)(&full_bbox.min[0]), sizeof(full_bbox.min[0]));
            full_bboxes_fs.write((char*)(&full_bbox.max[0]), sizeof(full_bbox.max[0]));
            full_bboxes_fs.write((char*)(&full_bbox.min[1]), sizeof(full_bbox.min[1]));
            full_bboxes_fs.write((char*)(&full_bbox.max[1]), sizeof(full_bbox.max[1]));
            full_bboxes_fs.write((char*)(&full_bbox.min[2]), sizeof(full_bbox.min[2]));
            full_bboxes_fs.write((char*)(&full_bbox.max[2]), sizeof(full_bbox.max[2]));
        }

        for (bbox_t& quant_bbox : quant_bboxes_by_cluster[i]) {
            quant_bboxes_fs.write((char*)(&quant_bbox.min[0]), sizeof(quant_bbox.min[0]));
            quant_bboxes_fs.write((char*)(&quant_bbox.max[0]), sizeof(quant_bbox.max[0]));
            quant_bboxes_fs.write((char*)(&quant_bbox.min[1]), sizeof(quant_bbox.min[1]));
            quant_bboxes_fs.write((char*)(&quant_bbox.max[1]), sizeof(quant_bbox.max[1]));
            quant_bboxes_fs.write((char*)(&quant_bbox.min[2]), sizeof(quant_bbox.min[2]));
            quant_bboxes_fs.write((char*)(&quant_bbox.max[2]), sizeof(quant_bbox.max[2]));
        }

        uint32_t size = trigs_by_cluster[i].size();
        trigs_size_fs.write((char*)(&size), sizeof(size));

        for (trig_t& trig : trigs_by_cluster[i]) {
            vector_t p1 = trig.p1();
            vector_t p2 = trig.p2();
            trigs_fs.write((char*)(&trig.p0[0]), sizeof(trig.p0[0]));
            trigs_fs.write((char*)(&trig.p0[1]), sizeof(trig.p0[1]));
            trigs_fs.write((char*)(&trig.p0[2]), sizeof(trig.p0[2]));
            trigs_fs.write((char*)(&p1[0]), sizeof(p1[0]));
            trigs_fs.write((char*)(&p1[1]), sizeof(p1[1]));
            trigs_fs.write((char*)(&p1[2]), sizeof(p1[2]));
            trigs_fs.write((char*)(&p2[0]), sizeof(p2[0]));
            trigs_fs.write((char*)(&p2[1]), sizeof(p2[1]));
            trigs_fs.write((char*)(&p2[2]), sizeof(p2[2]));
        }
    }
}

}  // namespace bvh_quantize

#endif //BUILD_HPP
