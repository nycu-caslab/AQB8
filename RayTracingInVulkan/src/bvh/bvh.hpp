#ifndef BVH_BVH_HPP
#define BVH_BVH_HPP

#include <climits>
#include <memory>
#include <cassert>
#include <stack>

#include "bvh/bounding_box.hpp"
#include "bvh/utilities.hpp"

namespace bvh
{
    /// This structure represents a BVH with a list of nodes and primitives indices.
    /// The memory layout is such that the children of a node are always grouped together.
    /// This means that each node only needs one index to point to its children, as the other
    /// child can be obtained by adding one to the index of the first child. The root of the
    /// hierarchy is located at index 0 in the array of nodes.

    template <typename Scalar>
    struct Bvh
    {
        using IndexType = typename SizedIntegerType<sizeof(Scalar) * CHAR_BIT>::Unsigned;
        using ScalarType = Scalar;

        struct Trig
        {
            float v[3][3];
        };

        struct Node_v2
        {
            float bounds[6][6];
            uint32_t data[6];
        };

        // The size of this structure should be 32 bytes in
        // single precision and 64 bytes in double precision.
        struct Node
        {
            Scalar bounds[6];
            IndexType primitive_count;
            IndexType first_child_or_primitive;

            bool is_leaf_val;
            bool is_leaf() const { return is_leaf_val; }
            // bool is_leaf() const { return primitive_count != 0; }

            /// Accessor to simplify the manipulation of the bounding box of a node.
            /// This type is convertible to a `BoundingBox`.
            struct BoundingBoxProxy
            {
                Node &node;

                BoundingBoxProxy(Node &node)
                    : node(node)
                {
                }

                BoundingBoxProxy &operator=(const BoundingBox<Scalar> &bbox)
                {
                    node.bounds[0] = bbox.min[0];
                    node.bounds[1] = bbox.max[0];
                    node.bounds[2] = bbox.min[1];
                    node.bounds[3] = bbox.max[1];
                    node.bounds[4] = bbox.min[2];
                    node.bounds[5] = bbox.max[2];
                    return *this;
                }

                operator BoundingBox<Scalar>() const
                {
                    return BoundingBox<Scalar>(
                        Vector3<Scalar>(node.bounds[0], node.bounds[2], node.bounds[4]),
                        Vector3<Scalar>(node.bounds[1], node.bounds[3], node.bounds[5]));
                }

                BoundingBox<Scalar> to_bounding_box() const
                {
                    return static_cast<BoundingBox<Scalar>>(*this);
                }

                Scalar half_area() const { return to_bounding_box().half_area(); }

                BoundingBoxProxy &extend(const BoundingBox<Scalar> &bbox)
                {
                    return *this = to_bounding_box().extend(bbox);
                }

                BoundingBoxProxy &extend(const Vector3<Scalar> &vector)
                {
                    return *this = to_bounding_box().extend(vector);
                }
            };

            BoundingBoxProxy bounding_box_proxy()
            {
                return BoundingBoxProxy(*this);
            }

            const BoundingBoxProxy bounding_box_proxy() const
            {
                return BoundingBoxProxy(*const_cast<Node *>(this));
            }
        };

        /// Given a node index, returns the index of its sibling.
        static size_t sibling(size_t index)
        {
            assert(index != 0);
            return index % 2 == 1 ? index + 1 : index - 1;
        }

        /// Returns true if the given node is the left sibling of another.
        static bool is_left_sibling(size_t index)
        {
            assert(index != 0);
            return index % 2 == 1;
        }

        std::unique_ptr<Node[]> nodes;
        std::unique_ptr<Node_v2[]> nodes_v2;
        std::unique_ptr<size_t[]> primitive_indices;

        size_t node_count = 0;
        size_t node_count_v2 = 0;

        inline uint32_t pack_child_info(bool is_leaf_val, size_t primitive_count, size_t first_child_or_primitive)
        {
            return (static_cast<uint32_t>(is_leaf_val) << 31) |
                   ((static_cast<uint32_t>(primitive_count) & 0x7) << 28) |
                   (static_cast<uint32_t>(first_child_or_primitive) & 0x0FFFFFFF);
        }

        inline void convert_nodes(const std::unique_ptr<Node[]> &old_nodes, size_t node_count)
        {
            std::vector<Node_v2> temp_nodes;

            std::vector<size_t> old_to_new_index(node_count, -1);
            size_t new_index = 0;

            // Step 1: Assign new indices to valid nodes
            for (size_t i = 0; i < node_count; i++)
            {
                if (!old_nodes[i].is_leaf())
                {
                    old_to_new_index[i] = new_index++;
                }
            }

            // Step 2: Fill new_nodes while updating child indices
            for (size_t i = 0; i < node_count; i++)
            {
                Node &old_n = old_nodes[i];

                if (old_n.is_leaf())
                    continue;

                Node_v2 new_n;

                for (int j = 0; j < old_n.primitive_count; j++)
                {
                    size_t child_index = old_n.first_child_or_primitive + j;
                    Node &child = old_nodes[child_index];

                    std::memcpy(new_n.bounds[j], child.bounds, sizeof(float) * 6);
                    if (child.is_leaf())
                    {
                        new_n.data[j] = pack_child_info(child.is_leaf_val, child.primitive_count, child.first_child_or_primitive);
                    }
                    else
                    {
                        new_n.data[j] = pack_child_info(child.is_leaf_val, child.primitive_count, old_to_new_index[child_index]);
                    }
                }

                temp_nodes.emplace_back(new_n);
            }

            std::unique_ptr<Node_v2[]> new_nodes = std::make_unique<Node_v2[]>(temp_nodes.size());
            std::copy(temp_nodes.begin(), temp_nodes.end(), new_nodes.get());

            this->nodes_v2 = std::move(new_nodes);
            this->node_count_v2 = temp_nodes.size();
        }
    };

} // namespace bvh

#endif
