#ifndef BVH_BVH_HPP
#define BVH_BVH_HPP

#include <memory>
#include <cassert>
#include <climits>
#include <cstdint>

#define BVH_ALIGNMENT 64
#define BVH_TRIG_length 36
#define BVH_NODE_length 168
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
            float bounds[6][6];
            uint32_t data[6];
        };

        std::unique_ptr<Trig[]> trigs;
        std::unique_ptr<Node[]> nodes;
        std::unique_ptr<size_t[]> primitive_indices;

        size_t node_count = 0;
    };

} // namespace bvh

#endif // BVH_BVH_HPP
