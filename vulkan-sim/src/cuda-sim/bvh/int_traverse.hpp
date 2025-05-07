#ifndef INT_TRAVERSE_HPP
#define INT_TRAVERSE_HPP

#include <array>
#include <cstdint>
#include <utility>
#include <algorithm>

namespace bvh_quantize
{
    struct cluster_data_t
    {
        uint16_t cluster_idx;
        int_node_t *local_nodes;
        int_trig_t *local_trigs;

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

}

#endif // INT_TRAVERSE_HPP
