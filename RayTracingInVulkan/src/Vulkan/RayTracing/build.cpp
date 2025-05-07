#include "build.hpp"

namespace bvh_quantize
{
    int32_t floor_to_int32(float x)
    {
        assert(!std::isnan(x));
        if (x < -2147483648.0f)
            return -2147483648;
        if (x >= 2147483648.0f)
            return 2147483647;
        return (int)floorf(x);
    }

    int32_t ceil_to_int32(float x)
    {
        assert(!std::isnan(x));
        if (x <= -2147483649.0f)
            return -2147483648;
        if (x > 2147483647.0f)
            return 2147483647;
        return (int)ceilf(x);
    }

    float get_scaling_factor(const bvh_t &bvh, size_t ref_idx)
    {
        bbox_t ref_bbox = bvh.nodes[ref_idx].bounding_box_proxy().to_bounding_box();

        float max_len = 0.0f;
        for (int i = 0; i < 3; i++)
            max_len = std::max(max_len, ref_bbox.max[i] - ref_bbox.min[i]);

        float scaling_factor = max_len / (float)qx_max;
        assert(scaling_factor > 0.0f);
        return scaling_factor;
    }

    // return: [qxmin, qxmax, qymin, qymax, qzmin, qzmax]
    std::array<uint8_t, 6> get_int_bounds(const bvh_t &bvh, size_t node_idx, size_t ref_idx, float scaling_factor)
    {
        bbox_t node_bbox = bvh.nodes[node_idx].bounding_box_proxy().to_bounding_box();
        bbox_t ref_bbox = bvh.nodes[ref_idx].bounding_box_proxy().to_bounding_box();

        std::array<uint8_t, 6> ret{};
        for (int i = 0; i < 3; i++)
        {
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

    bbox_t get_quant_bbox(const bvh_t &bvh, size_t node_idx, size_t ref_idx, float scaling_factor)
    {
        bbox_t ref_bbox = bvh.nodes[ref_idx].bounding_box_proxy().to_bounding_box();
        std::array<uint8_t, 6> int_bounds = get_int_bounds(bvh, node_idx, ref_idx, scaling_factor);

        bbox_t ret;
        for (int i = 0; i < 3; i++)
        {
            ret.min[i] = ref_bbox.min[i] + scaling_factor * (float)int_bounds[i * 2];
            ret.max[i] = ref_bbox.min[i] + scaling_factor * (float)int_bounds[i * 2 + 1];
            assert(std::isfinite(ret.min[i]));
            assert(std::isfinite(ret.max[i]));
        }
        return ret;
    }

    arg_t parse_arg(int argc, char *argv[])
    {
        if (argc < 5)
        {
            std::cerr << "usage: " << argv[0] << " MODEL_FILE T_TRV_INT T_SWITCH T_IST [RAY_FILE]" << std::endl;
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
        if (argc >= 6)
        {
            arg.ray_file = argv[5];
            std::cout << "RAY_FILE = " << arg.ray_file << std::endl;
        }

        return arg;
    }

    bvh_t build_bvh(const std::vector<trig_t> &trigs)
    {
        auto [bboxes, centers] = bvh::compute_bounding_boxes_and_centers(trigs.data(), trigs.size());
        auto global_bbox = bvh::compute_bounding_boxes_union(bboxes.get(), trigs.size());
        std::cout << "global_bbox = ("
                  << global_bbox.min[0] << ", " << global_bbox.min[1] << ", " << global_bbox.min[2] << "), ("
                  << global_bbox.max[0] << ", " << global_bbox.max[1] << ", " << global_bbox.max[2] << ")" << std::endl;

        // Building the BVH tree
        bvh_t bvh;
        builder_t builder(bvh);
        builder.max_leaf_size = max_trig_in_leaf_size;
        builder.build(global_bbox, bboxes.get(), centers.get(), trigs.size());

        return bvh;
    }

    std::vector<policy_t> get_policy(float t_trv_int, float t_switch, float t_ist, const bvh_t &bvh)
    {
        // stk_1: fill t_buf_size, t_buf_idx_map
        int t_buf_size = 0;
        std::vector<int> t_buf_idx_map(bvh.node_count);
        std::stack<std::pair<size_t, int>> stk_1;
        node_t &root_node = bvh.nodes[0];
        size_t root_left_node_idx = root_node.first_child_or_primitive;
        size_t root_right_node_idx = root_left_node_idx + 1;
        stk_1.emplace(root_right_node_idx, 1);
        stk_1.emplace(root_left_node_idx, 1);
        while (!stk_1.empty())
        {
            auto [curr_node_idx, depth] = stk_1.top();
            node_t &curr_node = bvh.nodes[curr_node_idx];
            stk_1.pop();

            t_buf_idx_map[curr_node_idx] = t_buf_size;
            t_buf_size += depth;

            if (!curr_node.is_leaf())
            {
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
        while (!stk_2.empty())
        {
            auto [curr_node_idx, first] = stk_2.top();
            node_t &curr_node = bvh.nodes[curr_node_idx];
            stk_2.pop();

            size_t left_node_idx = curr_node.first_child_or_primitive;
            size_t right_node_idx = left_node_idx + 1;

            if (first)
            {
                stk_2.emplace(curr_node_idx, false);
                if (!curr_node.is_leaf())
                {
                    stk_2.emplace(right_node_idx, true);
                    stk_2.emplace(left_node_idx, true);
                    parent[left_node_idx] = curr_node_idx;
                    parent[right_node_idx] = curr_node_idx;
                }
            }
            else
            {
                size_t ref_idx = parent[curr_node_idx];
                for (int i = 0;; i++)
                {
                    float scaling_factor = get_scaling_factor(bvh, ref_idx);
                    bbox_t quant_bbox = get_quant_bbox(bvh, curr_node_idx, ref_idx, scaling_factor);
                    float half_area = quant_bbox.half_area();

                    float &curr_t_buf = t_buf[t_buf_idx_map[curr_node_idx] + i];
                    policy_t &curr_t_policy = t_policy[t_buf_idx_map[curr_node_idx] + i];
                    if (curr_node.is_leaf())
                    {
                        curr_t_buf = t_ist * (float)curr_node.primitive_count * half_area;
                    }
                    else
                    {
                        float left_stay_t = t_buf[t_buf_idx_map[left_node_idx] + 1 + i];
                        float right_stay_t = t_buf[t_buf_idx_map[right_node_idx] + 1 + i];

                        float left_switch_t = t_buf[t_buf_idx_map[left_node_idx]];
                        float right_switch_t = t_buf[t_buf_idx_map[right_node_idx]];

                        float curr_stay_t = t_trv_int * 2 * half_area + left_stay_t + right_stay_t;
                        float curr_switch_t = (t_trv_int * 2 + t_switch) * half_area + left_switch_t + right_switch_t;

                        assert(std::isfinite(curr_stay_t));
                        assert(std::isfinite(curr_switch_t));

                        if (curr_switch_t < curr_stay_t)
                        {
                            curr_t_buf = curr_switch_t;
                            curr_t_policy = policy_t::SWITCH;
                        }
                        else
                        {
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
        while (!stk_3.empty())
        {
            auto [curr_node_idx, curr_offset] = stk_3.top();
            node_t &curr_node = bvh.nodes[curr_node_idx];
            stk_3.pop();

            if (curr_node.is_leaf())
                continue;

            size_t left_node_idx = curr_node.first_child_or_primitive;
            size_t right_node_idx = left_node_idx + 1;

            switch (t_policy[t_buf_idx_map[curr_node_idx] + curr_offset])
            {
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

    int_bvh_t build_int_bvh(float t_trv_int, float t_switch, float t_ist, const std::vector<trig_t> &trigs,
                            const bvh_t &bvh)
    {
        // arg.t_trv_int    = 0.5
        // arg.t_switch     = 1
        // arg.t_ist        = 1

        // fill policy
        std::vector<policy_t> policy = get_policy(t_trv_int, t_switch, t_ist, bvh);

        // que: fill num_clusters, cluster_node_indices, ref_indices, local_node_idx_map
        int num_clusters = 0;
        std::vector<std::vector<size_t>> cluster_node_indices;
        std::vector<size_t> ref_indices;
        std::vector<size_t> local_node_idx_map(bvh.node_count);
        std::queue<std::pair<size_t, int>> que;
        que.emplace(0, -1);
        while (!que.empty())
        {
            auto [curr_node_idx, curr_cluster_idx] = que.front();
            node_t &curr_node = bvh.nodes[curr_node_idx];
            que.pop();

            if (curr_node.is_leaf())
                continue;

            int child_cluster_idx = -1;
            switch (policy[curr_node_idx])
            {
            case policy_t::STAY: // 留在當前集群
                child_cluster_idx = curr_cluster_idx;
                break;
            case policy_t::SWITCH: // 切換到新集群
                child_cluster_idx = num_clusters;
                num_clusters++;
                cluster_node_indices.emplace_back();
                ref_indices.push_back(curr_node_idx);
                break;
            }

            // 遍歷所有節點，根據策略 policy 決定當前節點應該留在當前集群 (STAY) 還是切換到新集群 (SWITCH)。
            // 更新局部索引映射 local_node_idx_map 和集群節點索引 cluster_node_indices。
            // 如果節點是內部節點，將其子節點加入隊列 que。

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
        for (int i = 0; i < num_clusters; i++)
        {
            for (unsigned int j = 0; j < cluster_node_indices[i].size(); j++)
                cluster_idx_map[cluster_node_indices[i][j]] = i;
            scaling_factors[i] = get_scaling_factor(bvh, ref_indices[i]);

            // 初始化 cluster_idx_map 和 scaling_factors。
            // 對於每個集群，更新節點索引映射 cluster_idx_map 和計算縮放因子 scaling_factors。
        }

        // fill int_bvh, local_trig_idx_map
        int_bvh_t int_bvh;
        int_bvh.num_clusters = num_clusters;
        int_bvh.clusters = std::make_unique<int_cluster_t[]>(num_clusters);
        int_bvh.trigs = std::make_unique<trig_t[]>(trigs.size());
        int_bvh.nodes = std::make_unique<int_node_t[]>(bvh.node_count);
        int_bvh.nodes_v2 = std::make_unique<int_node_v2_t[]>(bvh.node_count);
        int_bvh.primitive_indices = std::make_unique<size_t[]>(trigs.size());

        std::vector<size_t> local_trig_idx_map(bvh.node_count);
        size_t tmp_node_offset = 0;
        size_t tmp_trig_offset = 0;
        for (int i = 0; i < num_clusters; i++)
        {
            // fill int_bvh.clusters[i]
            for (int j = 0; j < 6; j++)
                int_bvh.clusters[i].ref_bounds[j] = bvh.nodes[ref_indices[i]].bounds[j];
            int_bvh.clusters[i].inv_sx_inv_sw = inv_sw / scaling_factors[i];
            int_bvh.clusters[i].node_offset = tmp_node_offset;
            int_bvh.clusters[i].trig_offset = tmp_trig_offset;

            // fill int_bvh.trigs
            int tmp_local_trig_offset = 0;
            for (size_t curr_node_idx : cluster_node_indices[i])
            {
                node_t &curr_node = bvh.nodes[curr_node_idx];
                if (curr_node.is_leaf())
                {
                    local_trig_idx_map[curr_node_idx] = tmp_local_trig_offset;
                    for (unsigned int j = 0; j < curr_node.primitive_count; j++)
                    {
                        size_t trig_idx = bvh.primitive_indices[curr_node.first_child_or_primitive + j];
                        int_bvh.trigs[tmp_trig_offset] = trigs[trig_idx];
                        int_bvh.primitive_indices[tmp_trig_offset] = trig_idx;

                        tmp_trig_offset++;
                        tmp_local_trig_offset++;
                    }
                }
            }

            // fill int_bvh.nodes
            for (size_t curr_node_idx : cluster_node_indices[i])
            {
                node_t &curr_node = bvh.nodes[curr_node_idx];
                int_node_t &curr_int_node = int_bvh.nodes[tmp_node_offset];

                // fill curr_int_node.bounds
                std::array<uint8_t, 6> bounds = get_int_bounds(bvh, curr_node_idx, ref_indices[i], scaling_factors[i]);
                for (int j = 0; j < 6; j++)
                    curr_int_node.bounds[j] = bounds[j];

                child_type_t child_type;
                size_t left_node_idx = curr_node.first_child_or_primitive;
                size_t right_node_idx = left_node_idx + 1;
                if (curr_node.is_leaf())
                {
                    child_type = child_type_t::LEAF;
                }
                else
                {
                    int curr_cluster_idx = cluster_idx_map[curr_node_idx];
                    int left_cluster_idx = cluster_idx_map[left_node_idx];
                    int right_cluster_idx = cluster_idx_map[right_node_idx];
                    assert(left_cluster_idx == right_cluster_idx);

                    if (curr_cluster_idx != left_cluster_idx)
                    {
                        assert(policy[curr_node_idx] == policy_t::SWITCH);
                        child_type = child_type_t::SWITCH;
                    }
                    else
                    {
                        assert(policy[curr_node_idx] == policy_t::STAY);
                        child_type = child_type_t::INTERNAL;
                    }
                }

                // fill curr_int_node.data
                switch (child_type)
                {
                case child_type_t::INTERNAL:
                {
                    size_t field_c = local_node_idx_map[left_node_idx];
                    if (field_c >= max_node_in_cluster_size)
                    {
                        std::cerr << "internal node cannot fit!" << std::endl;
                        exit(EXIT_FAILURE);
                    }
                    curr_int_node.data = 0x8000 | field_c;
                    break;
                }
                case child_type_t::LEAF:
                {
                    size_t field_b = bvh.nodes[curr_node_idx].primitive_count;
                    size_t field_c = local_trig_idx_map[curr_node_idx];
                    if (field_b > max_trig_in_leaf_size || field_c >= max_trig_in_cluster_size)
                    {
                        std::cerr << "leaf node cannot fit!" << std::endl;
                        exit(EXIT_FAILURE);
                    }
                    curr_int_node.data = 0x8000 | (field_b << field_c_bits) | field_c;
                    break;
                }
                case child_type_t::SWITCH:
                {
                    size_t field_bc = cluster_idx_map[left_node_idx];
                    if (field_bc >= max_cluster_size)
                    {
                        std::cerr << "switch node cannot fit!" << std::endl;
                        exit(EXIT_FAILURE);
                    }
                    curr_int_node.data = field_bc;
                    break;
                }
                }
                tmp_node_offset++;
            }
        }

        return int_bvh;
    }

    decoded_data_t decode_data(uint16_t data)
    {
        decoded_data_t decoded_data{};
        if (data & 0x8000)
        {
            int field_b = ((data & 0x7fff) >> field_c_bits);
            int field_c = (data & ((1 << field_c_bits) - 1));
            if (field_b == 0)
            {
                decoded_data.child_type = child_type_t::INTERNAL;
            }
            else
            {
                decoded_data.child_type = child_type_t::LEAF;
                decoded_data.num_trigs = field_b;
            }
            decoded_data.idx = field_c;
        }
        else
        {
            decoded_data.child_type = child_type_t::SWITCH;
            decoded_data.idx = data;
        }
        return decoded_data;
    }

    int_bvh_v2_t convert_nodes(const int_bvh_t &int_bvh, const bvh_t &bvh, const std::vector<trig_t> &trigs)
    {
        int_bvh_v2_t int_bvh_v2;

        int_bvh_v2.num_clusters = int_bvh.num_clusters;
        int_bvh_v2.clusters = std::make_unique<int_cluster_t[]>(int_bvh.num_clusters);
        std::copy(int_bvh.clusters.get(), int_bvh.clusters.get() + int_bvh.num_clusters, int_bvh_v2.clusters.get());
        int_bvh_v2.trigs = std::make_unique<trig_t[]>(trigs.size());
        std::copy(int_bvh.trigs.get(), int_bvh.trigs.get() + trigs.size(), int_bvh_v2.trigs.get());
        int_bvh_v2.nodes_v2 = std::make_unique<int_node_v2_t[]>(bvh.node_count);
        std::copy(int_bvh.nodes_v2.get(), int_bvh.nodes_v2.get() + bvh.node_count, int_bvh_v2.nodes_v2.get());
        int_bvh_v2.primitive_indices = std::make_unique<size_t[]>(trigs.size());
        std::copy(int_bvh.primitive_indices.get(), int_bvh.primitive_indices.get() + trigs.size(), int_bvh_v2.primitive_indices.get());

        // Fill new_nodes while updating child indices
        for (size_t i = 0; i < bvh.node_count; i++)
        {
            int_node_v2_t &curr_node_v2 = int_bvh_v2.nodes_v2[i];

            uint16_t left_child_data = curr_node_v2.left_child_data;
            decoded_data_t left_decoded_data = decode_data(left_child_data);

            size_t left_child_index = i;
            size_t right_child_index = i + 1;

            int_node_t &left_child = int_bvh.nodes[left_child_index];
            int_node_t &right_child = int_bvh.nodes[right_child_index];

            std::memcpy(curr_node_v2.left_bounds, left_child.bounds, sizeof(uint8_t) * 6);
            std::memcpy(curr_node_v2.right_bounds, right_child.bounds, sizeof(uint8_t) * 6);
            curr_node_v2.left_child_data = left_child.data;
            curr_node_v2.right_child_data = right_child.data;
        }

        return int_bvh_v2;
    }
}
