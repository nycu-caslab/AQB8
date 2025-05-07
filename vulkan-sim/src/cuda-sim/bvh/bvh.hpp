#ifndef BVH_BVH_HPP
#define BVH_BVH_HPP

#include <memory>
#include <cassert>
#include <climits>
#include <cstdint>

#define COMPRESS_BVH_ALIGNMENT 64
#define COMPRESS_BVH_TRIG_length 36
#define COMPRESS_BVH_NODE_length 64
#define COMPRESS_BVH_ROOT_length 56
#define COMPRESS_BVH_PRIMITIVE_INSTANCE_length 4

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
            uint8_t bounds_quant[6][6];
            int8_t exp[3];
            uint32_t data[6];
        };

        struct Root
        {
            float bounds[6];
            uint8_t bounds_quant[6];
            int8_t exp[3];
            size_t primitive_count;
            size_t first_child_or_primitive;

            bool is_leaf() const { return primitive_count != 0; }
        };

        std::unique_ptr<Trig[]> trigs;
        std::unique_ptr<Node[]> nodes;
        std::unique_ptr<size_t[]> primitive_indices;
        Root root_node;

        size_t node_count = 0;
    };

} // namespace bvh

#endif // BVH_BVH_HPP
