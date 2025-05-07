#ifndef BVH_BVH_HPP
#define BVH_BVH_HPP

#include <memory>
#include <cassert>
#include <climits>
#include <cstdint>

#define BVH_ALIGNMENT 64
#define BVH_TRIG_length 36
#define BVH_NODE_length 56
#define BVH_PRIMITIVE_INSTANCE_length 4

namespace bvh
{
    template <typename Scalar>
    struct Bvh
    {
        struct Trig
        {
            float v[3][3];
        };

        struct Node
        {
            float left_bounds[6];
            float right_bounds[6];
            uint32_t left_child_data;
            uint32_t right_child_data;
        };

        std::unique_ptr<Trig[]> trigs;
        std::unique_ptr<Node[]> nodes;
        std::unique_ptr<size_t[]> primitive_indices;

        size_t node_count = 0;
    };

} // namespace bvh

#endif // BVH_BVH_HPP
