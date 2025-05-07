#ifndef TRAVERSE_HPP
#define TRAVERSE_HPP

namespace bvh_quantize
{

    typedef bvh::Ray<float> ray_t;
    typedef trig_t::Intersection intersection_t;

    struct cluster_data_t
    {
        uint16_t cluster_idx;
        int_node_t *local_nodes;
        trig_t *local_trigs;

        uint32_t node_offset;
        uint32_t trig_offset;

        float inv_sx_inv_sw;
        float y_ref;
        int32_t qb_l[3];
        int32_t qb_h[3];
        uint8_t tmax_version;
        int32_t qy_max;
        uint8_t num_nodes_in_stk_2;
        uint32_t num_nodes;
    };

    struct cluster_data_v2_t
    {
        uint16_t cluster_idx;
        int_node_v2_t *local_nodes;
        trig_t *local_trigs;

        uint32_t node_offset;
        uint32_t trig_offset;

        float inv_sx_inv_sw;
        float y_ref;
        int32_t qb_l[3];
        int32_t qb_h[3];
        uint8_t tmax_version;
        int32_t qy_max;
        uint8_t num_nodes_in_stk_2;
        uint32_t num_nodes;
    };

    struct int_w_t
    {
        bool iw[3];
        uint8_t rw_l[3];
        uint8_t qw_l[3];
        uint8_t rw_h[3];
        uint8_t qw_h[3];
    };

    struct statistics_t
    {
        struct
        {
            size_t intersections_a = 0;
            size_t intersections_b = 0;
            struct Empty
            {
                Empty &operator++() { return *this; }
            } traversal_steps, both_intersected, finalize;
        } bvh_statistics;
        uintmax_t intersect_bbox = 0;
        uintmax_t push_cluster = 0;
        uintmax_t recompute_qymax = 0;
        uintmax_t traversal_steps = 0;
        uintmax_t both_intersected = 0;
        uintmax_t finalize = 0;
    };

    int_w_t get_int_w(const std::array<float, 3> &w)
    {
        int_w_t int_w{};

        for (int i = 0; i < 3; i++)
        {
            auto &wi = reinterpret_cast<const uint32_t &>(w[i]);

            bool sign = wi & 0x80000000;
            uint32_t exponent = (wi >> 23) & 0xff;
            uint32_t mantissa = wi & 0x7fffff;

            // 127 + 7 = 134
            uint32_t near_exponent = (exponent - 127) & 0b11111;
            uint32_t near_mantissa = mantissa >> 16;
            uint32_t near = (near_exponent << 7) | near_mantissa;

            uint32_t far = near + 1;
            uint32_t far_exponent = far >> 7;
            uint32_t far_mantissa = far & 0b1111111;

            int_w.iw[i] = sign;
            near_mantissa |= 0b10000000;
            far_mantissa |= 0b10000000;

            if (sign)
            {
                int_w.rw_l[i] = far_exponent;
                int_w.qw_l[i] = far_mantissa;
                int_w.rw_h[i] = near_exponent;
                int_w.qw_h[i] = near_mantissa;
            }
            else
            {
                int_w.rw_l[i] = near_exponent;
                int_w.qw_l[i] = near_mantissa;
                int_w.rw_h[i] = far_exponent;
                int_w.qw_h[i] = far_mantissa;
            }
        }

        return int_w;
    }

    std::optional<intersection_t> intersect_leaf(const decoded_data_t &decoded_data, const cluster_data_t &curr_cluster, ray_t &ray, const int_bvh_t &int_bvh, statistics_t &statistics)
    {
        assert(decoded_data.child_type == child_type_t::LEAF);
        std::optional<intersection_t> best_hit;

        for (int i = 0; i < decoded_data.num_trigs; i++)
        {
            uint32_t primID = curr_cluster.trig_offset + decoded_data.idx + i;
            trig_t *tmp_trigs = &int_bvh.trigs[primID];

            if (auto hit = tmp_trigs->intersect(ray, statistics.bvh_statistics))
            {
                best_hit = hit.value();
                ray.tmax = hit->t;
            }
        }

        return best_hit;
    }

    std::optional<float> intersect_bbox(const std::array<bool, 3> &octant,
                                        const std::array<float, 3> &w,
                                        const float *x,
                                        const std::array<float, 3> &b,
                                        float tmax)
    {
        float entry[3], exit[3];
        entry[0] = w[0] * x[0 + octant[0]] + b[0];
        entry[1] = w[1] * x[2 + octant[1]] + b[1];
        entry[2] = w[2] * x[4 + octant[2]] + b[2];
        exit[0] = w[0] * x[1 - octant[0]] + b[0];
        exit[1] = w[1] * x[3 - octant[1]] + b[1];
        exit[2] = w[2] * x[5 - octant[2]] + b[2];

        float entry_ = fmaxf(entry[0], fmaxf(entry[1], fmaxf(entry[2], 0.0f)));
        float exit_ = fminf(exit[0], fminf(exit[1], fminf(exit[2], tmax)));

        if (entry_ <= exit_)
            return entry_;
        else
            return std::nullopt;
    }

    std::optional<int32_t> intersect_int_bbox(int32_t qy_max,
                                              const int_w_t &int_w,
                                              const uint8_t *qx,
                                              const int32_t *qb_l,
                                              const int32_t *qb_h)
    {
        const int32_t qx_a[3] = {// we use int32_t instead of uint8_t just for convenience
                                 int_w.iw[0] ? qx[1] : qx[0],
                                 int_w.iw[1] ? qx[3] : qx[2],
                                 int_w.iw[2] ? qx[5] : qx[4]};

        const int32_t qx_b[3] = {
            int_w.iw[0] ? qx[0] : qx[1],
            int_w.iw[1] ? qx[2] : qx[3],
            int_w.iw[2] ? qx[4] : qx[5]};

        int32_t entry[3];
        int32_t exit[3];
        entry[0] = (int_w.qw_l[0] * qx_a[0]) << int_w.rw_l[0];
        entry[0] = (int_w.iw[0] ? -entry[0] : entry[0]) + qb_l[0];
        entry[1] = (int_w.qw_l[1] * qx_a[1]) << int_w.rw_l[1];
        entry[1] = (int_w.iw[1] ? -entry[1] : entry[1]) + qb_l[1];
        entry[2] = (int_w.qw_l[2] * qx_a[2]) << int_w.rw_l[2];
        entry[2] = (int_w.iw[2] ? -entry[2] : entry[2]) + qb_l[2];
        exit[0] = (int_w.qw_h[0] * qx_b[0]) << int_w.rw_h[0];
        exit[0] = (int_w.iw[0] ? -exit[0] : exit[0]) + qb_h[0];
        exit[1] = (int_w.qw_h[1] * qx_b[1]) << int_w.rw_h[1];
        exit[1] = (int_w.iw[1] ? -exit[1] : exit[1]) + qb_h[1];
        exit[2] = (int_w.qw_h[2] * qx_b[2]) << int_w.rw_h[2];
        exit[2] = (int_w.iw[2] ? -exit[2] : exit[2]) + qb_h[2];

        int32_t entry_ = std::max(entry[0], std::max(entry[1], std::max(entry[2], 0)));
        int32_t exit_ = std::min(exit[0], std::min(exit[1], std::min(exit[2], qy_max)));

        if (entry_ <= exit_)
            return entry_;
        else
            return std::nullopt;
    }

    std::optional<intersection_t> int_traverse(int_bvh_t &int_bvh, trig_t *trigs, ray_t ray, statistics_t &statistics)
    {
        std::optional<intersection_t> best_hit;

        // preprocess ray
        assert(ray.tmin == 0.0f);
        std::array<bool, 3> octant = {
            std::signbit(ray.direction[0]),
            std::signbit(ray.direction[1]),
            std::signbit(ray.direction[2])};
        std::array<float, 3> w = {
            1.0f / ray.direction[0],
            1.0f / ray.direction[1],
            1.0f / ray.direction[2]};
        std::array<float, 3> b = {
            -ray.origin[0] * w[0],
            -ray.origin[1] * w[1],
            -ray.origin[2] * w[2]};
        int_w_t int_w = get_int_w(w);
        uint8_t global_tmax_version = 0;

        cluster_data_t cluster_data = {
            .num_nodes_in_stk_2 = 0};
        std::stack<cluster_data_t> stk_1;
        std::stack<std::pair<uint16_t, uint16_t>> stk_2; // [local_node_idx, cluster_idx]

        auto update_cluster_data = [&](uint16_t cluster_idx) -> bool
        {
            statistics.intersect_bbox++;
            int_cluster_t cluster = int_bvh.clusters[cluster_idx];
            auto y_ref = intersect_bbox(octant, w, cluster.ref_bounds, b, ray.tmax);
            if (!y_ref.has_value())
                return false;

            if (cluster_data.num_nodes_in_stk_2 != 0)
                stk_1.push(cluster_data);

            statistics.push_cluster++;
            cluster_data.cluster_idx = cluster_idx;
            cluster_data.local_nodes = &int_bvh.nodes[cluster.node_offset];
            cluster_data.local_trigs = &int_bvh.trigs[cluster.trig_offset];

            cluster_data.node_offset = cluster.node_offset;
            cluster_data.trig_offset = cluster.trig_offset;

            cluster_data.inv_sx_inv_sw = cluster.inv_sx_inv_sw;
            cluster_data.y_ref = y_ref.value();

            for (int i = 0; i < 3; i++)
            {
                // float o_local = ray.origin[i] + y_ref.value() * ray.direction[i] - cluster.ref_bounds[2 * i];
                // cluster_data.qb_l[i] = floor_to_int32(-o_local * w[i] * cluster_data.inv_sx_inv_sw);
                cluster_data.qb_l[i] = floor_to_int32((b[i] - y_ref.value() + cluster.ref_bounds[2 * i] * w[i]) *
                                                      cluster_data.inv_sx_inv_sw);
                cluster_data.qb_h[i] = cluster_data.qb_l[i] + 1;
            }
            cluster_data.tmax_version = global_tmax_version;
            cluster_data.qy_max = ceil_to_int32((ray.tmax - cluster_data.y_ref) * cluster_data.inv_sx_inv_sw);
            cluster_data.num_nodes_in_stk_2 = 0;
            cluster_data.num_nodes = cluster.num_nodes;
            return true;
        };

        // intersect root cluster
        if (!update_cluster_data(0))
            return std::nullopt;

        uint16_t left_local_node_idx = 0;
        auto update_node_and_cluster = [&](const decoded_data_t &decoded_data) -> bool
        {
            switch (decoded_data.child_type)
            {
            case child_type_t::INTERNAL:
                left_local_node_idx = decoded_data.idx;
                return true;
            case child_type_t::SWITCH:
                left_local_node_idx = 0;
                return update_cluster_data(decoded_data.idx);
            default:
                assert(false);
            }
        };

        auto print_data = [](const decoded_data_t &d)
        {
            std::cout << "child_type: " << static_cast<int>(d.child_type) << ", ";
            std::cout << "num_trigs: " << static_cast<int>(d.num_trigs) << ", ";
            std::cout << "idx: " << d.idx << std::endl;
        };

        auto print_bound = [](const int_node_t &n)
        {
            std::cout << "bounds: ";
            for (int i = 0; i < 6; i++)
            {
                std::cout << static_cast<int>(n.bounds[i]) << " ";
            }
        };

        while (true)
        {
            int_node_t nodes[6];
            decoded_data_t decoded_data[6];

            // optional, but can reduce traversal steps
            if (cluster_data.tmax_version != global_tmax_version)
            {
                statistics.recompute_qymax++;
                cluster_data.tmax_version = global_tmax_version;
                cluster_data.qy_max = ceil_to_int32((ray.tmax - cluster_data.y_ref) * cluster_data.inv_sx_inv_sw);
            }

            statistics.traversal_steps++;
            std::vector<std::pair<float, int>> sorted_nodes;

            for (int i = 0; i < 6; i++)
            {
                if (left_local_node_idx + i >= cluster_data.num_nodes)
                    break;

                nodes[i] = cluster_data.local_nodes[left_local_node_idx + i];

                auto distance = intersect_int_bbox(cluster_data.qy_max, int_w, nodes[i].bounds,
                                                   cluster_data.qb_l, cluster_data.qb_h);
                decoded_data[i] = decode_data(nodes[i].data);

                if (distance.has_value())
                {
                    if (decoded_data[i].child_type == child_type_t::LEAF)
                    {
                        if (auto hit = intersect_leaf(decoded_data[i], cluster_data, ray, int_bvh, statistics))
                        {
                            best_hit = hit;
                            global_tmax_version++;
                        }
                    }
                    else
                    {
                        sorted_nodes.emplace_back(distance.value(), i);
                    }
                }
            }

            std::sort(sorted_nodes.begin(), sorted_nodes.end());

            if (sorted_nodes.size() > 1)
                statistics.both_intersected++;

            for (size_t i = 1; i < sorted_nodes.size(); i++)
            {
                int idx = sorted_nodes[i].second;

                switch (decoded_data[idx].child_type)
                {
                case child_type_t::INTERNAL:
                    cluster_data.num_nodes_in_stk_2++;
                    stk_2.emplace(decoded_data[idx].idx, cluster_data.cluster_idx);
                    break;
                case child_type_t::SWITCH:
                    stk_2.emplace(0, decoded_data[idx].idx);
                    break;
                default:
                    assert(false);
                }
            }

            if (sorted_nodes.size() > 0)
            {
                int idx = sorted_nodes[0].second;
                if (update_node_and_cluster(decoded_data[idx]))
                    continue;
            }

            // pop from stk_2 until we found a valid node
            while (true)
            {
                if (stk_2.empty())
                    goto end;
                left_local_node_idx = stk_2.top().first;
                int cluster_idx = stk_2.top().second;
                stk_2.pop();
                if (cluster_data.cluster_idx == cluster_idx)
                {
                    cluster_data.num_nodes_in_stk_2--;
                    break;
                }
                if ((!stk_1.empty() && stk_1.top().cluster_idx == cluster_idx))
                {
                    cluster_data = stk_1.top();
                    stk_1.pop();
                    cluster_data.num_nodes_in_stk_2--;
                    break;
                }
                if (update_cluster_data(cluster_idx))
                    break;
            }
        }

    end:
        assert(stk_1.empty());
        if (best_hit.has_value())
            statistics.finalize++;
        return best_hit;
    }

    std::optional<intersection_t> intersect_leaf_v2(const decoded_data_t &decoded_data, const cluster_data_v2_t &curr_cluster, ray_t &ray, const int_bvh_v2_t &int_bvh_v2, statistics_t &statistics)
    {
        assert(decoded_data.child_type == child_type_t::LEAF);
        std::optional<intersection_t> best_hit;

        for (int i = 0; i < decoded_data.num_trigs; i++)
        {
            uint32_t primID = curr_cluster.trig_offset + decoded_data.idx + i;
            trig_t *tmp_trigs = &int_bvh_v2.trigs[primID];

            if (auto hit = tmp_trigs->intersect(ray, statistics.bvh_statistics))
            {
                best_hit = hit.value();
                ray.tmax = hit->t;
            }
        }

        return best_hit;
    }

    std::optional<intersection_t> int_traverse_v2(int_bvh_v2_t &int_bvh_v2, trig_t *trigs, ray_t ray, statistics_t &statistics)
    {
        std::optional<intersection_t> best_hit;

        // preprocess ray
        assert(ray.tmin == 0.0f);
        std::array<bool, 3> octant = {
            std::signbit(ray.direction[0]),
            std::signbit(ray.direction[1]),
            std::signbit(ray.direction[2])};
        std::array<float, 3> w = {
            1.0f / ray.direction[0],
            1.0f / ray.direction[1],
            1.0f / ray.direction[2]};
        std::array<float, 3> b = {
            -ray.origin[0] * w[0],
            -ray.origin[1] * w[1],
            -ray.origin[2] * w[2]};
        int_w_t int_w = get_int_w(w);
        uint8_t global_tmax_version = 0;

        cluster_data_v2_t cluster_data = {
            .num_nodes_in_stk_2 = 0};
        std::stack<cluster_data_v2_t> stk_1;
        std::stack<std::pair<uint16_t, uint16_t>> stk_2; // [local_node_idx, cluster_idx]

        auto update_cluster_data = [&](uint16_t cluster_idx) -> bool
        {
            statistics.intersect_bbox++;
            int_cluster_t cluster = int_bvh_v2.clusters[cluster_idx];
            auto y_ref = intersect_bbox(octant, w, cluster.ref_bounds, b, ray.tmax);
            if (!y_ref.has_value())
                return false;

            if (cluster_data.num_nodes_in_stk_2 != 0)
                stk_1.push(cluster_data);

            statistics.push_cluster++;
            cluster_data.cluster_idx = cluster_idx;
            cluster_data.local_nodes = &int_bvh_v2.nodes_v2[cluster.node_offset];
            cluster_data.local_trigs = &int_bvh_v2.trigs[cluster.trig_offset];

            cluster_data.node_offset = cluster.node_offset;
            cluster_data.trig_offset = cluster.trig_offset;

            cluster_data.inv_sx_inv_sw = cluster.inv_sx_inv_sw;
            cluster_data.y_ref = y_ref.value();

            for (int i = 0; i < 3; i++)
            {
                // float o_local = ray.origin[i] + y_ref.value() * ray.direction[i] - cluster.ref_bounds[2 * i];
                // cluster_data.qb_l[i] = floor_to_int32(-o_local * w[i] * cluster_data.inv_sx_inv_sw);
                cluster_data.qb_l[i] = floor_to_int32((b[i] - y_ref.value() + cluster.ref_bounds[2 * i] * w[i]) *
                                                      cluster_data.inv_sx_inv_sw);
                cluster_data.qb_h[i] = cluster_data.qb_l[i] + 1;
            }
            cluster_data.tmax_version = global_tmax_version;
            cluster_data.qy_max = ceil_to_int32((ray.tmax - cluster_data.y_ref) * cluster_data.inv_sx_inv_sw);
            cluster_data.num_nodes_in_stk_2 = 0;
            cluster_data.num_nodes = cluster.num_nodes;
            return true;
        };

        // intersect root cluster
        if (!update_cluster_data(0))
            return std::nullopt;

        uint16_t curr_local_node_idx = 0;
        auto update_node_and_cluster = [&](const decoded_data_t &decoded_data) -> bool
        {
            switch (decoded_data.child_type)
            {
            case child_type_t::INTERNAL:
                curr_local_node_idx = decoded_data.idx;
                return true;
            case child_type_t::SWITCH:
                curr_local_node_idx = 0;
                return update_cluster_data(decoded_data.idx);
            default:
                assert(false);
            }
        };

        auto print_data = [](const decoded_data_t &d)
        {
            std::cout << "child_type: " << static_cast<int>(d.child_type) << ", ";
            std::cout << "num_trigs: " << static_cast<int>(d.num_trigs) << ", ";
            std::cout << "idx: " << d.idx << std::endl;
        };

        auto print_bound = [](const uint8_t *bounds)
        {
            std::cout << "bounds: ";
            for (int i = 0; i < 6; i++)
            {
                std::cout << static_cast<int>(bounds[i]) << " ";
            }
        };

        while (true)
        {
            statistics.traversal_steps++;

            int_node_v2_t *curr_node = &cluster_data.local_nodes[curr_local_node_idx];
            decoded_data_t decoded_data[6];

            // optional, but can reduce traversal steps
            if (cluster_data.tmax_version != global_tmax_version)
            {
                statistics.recompute_qymax++;
                cluster_data.tmax_version = global_tmax_version;
                cluster_data.qy_max = ceil_to_int32((ray.tmax - cluster_data.y_ref) * cluster_data.inv_sx_inv_sw);
            }

            std::vector<std::pair<float, int>> sorted_nodes;

            for (int i = 0; i < 6; i++)
            {
                if (curr_local_node_idx + i >= cluster_data.num_nodes)
                    break;

                auto distance = intersect_int_bbox(cluster_data.qy_max, int_w, curr_node->bounds[i],
                                                   cluster_data.qb_l, cluster_data.qb_h);
                decoded_data[i] = decode_data(curr_node->data[i]);

                if (distance.has_value())
                {
                    if (decoded_data[i].child_type == child_type_t::LEAF)
                    {
                        if (auto hit = intersect_leaf_v2(decoded_data[i], cluster_data, ray, int_bvh_v2, statistics))
                        {
                            best_hit = hit;
                            global_tmax_version++;
                        }
                    }
                    else
                    {
                        sorted_nodes.emplace_back(distance.value(), i);
                    }
                }
            }

            std::sort(sorted_nodes.begin(), sorted_nodes.end());

            if (sorted_nodes.size() > 1)
                statistics.both_intersected++;

            for (size_t i = 1; i < sorted_nodes.size(); i++)
            {
                int idx = sorted_nodes[i].second;

                switch (decoded_data[idx].child_type)
                {
                case child_type_t::INTERNAL:
                    cluster_data.num_nodes_in_stk_2++;
                    stk_2.emplace(decoded_data[idx].idx, cluster_data.cluster_idx);
                    break;
                case child_type_t::SWITCH:
                    stk_2.emplace(0, decoded_data[idx].idx);
                    break;
                default:
                    assert(false);
                }
            }

            if (sorted_nodes.size() > 0)
            {
                int idx = sorted_nodes[0].second;
                if (update_node_and_cluster(decoded_data[idx]))
                    continue;
            }

            // pop from stk_2 until we found a valid node
            while (true)
            {
                if (stk_2.empty())
                    goto end;
                curr_local_node_idx = stk_2.top().first;
                int cluster_idx = stk_2.top().second;
                stk_2.pop();
                if (cluster_data.cluster_idx == cluster_idx)
                {
                    cluster_data.num_nodes_in_stk_2--;
                    break;
                }
                if ((!stk_1.empty() && stk_1.top().cluster_idx == cluster_idx))
                {
                    cluster_data = stk_1.top();
                    stk_1.pop();
                    cluster_data.num_nodes_in_stk_2--;
                    break;
                }
                if (update_cluster_data(cluster_idx))
                    break;
            }
        }

    end:
        assert(stk_1.empty());
        if (best_hit.has_value())
            statistics.finalize++;
        return best_hit;
    }

} // namespace bvh_quantize

#endif // TRAVERSE_HPP
