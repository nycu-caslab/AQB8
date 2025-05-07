#ifndef BVH_BVH_HPP
#define BVH_BVH_HPP

#include <climits>
#include <memory>
#include <cassert>

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

        // The size of this structure should be 32 bytes in
        // single precision and 64 bytes in double precision.
        struct Node
        {
            Scalar bounds[6];
            IndexType primitive_count;
            IndexType first_child_or_primitive;

            bool is_leaf() const { return primitive_count != 0; }

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
        std::unique_ptr<size_t[]> primitive_indices;
        size_t node_count = 0;

        size_t calculate_data_size() const
        {
            size_t node_size = sizeof(Node);
            size_t primitive_index_size = sizeof(size_t);
            size_t total_node_data_size = node_size * node_count;
            size_t total_primitive_indices_size = primitive_index_size * node_count;
            return total_node_data_size + total_primitive_indices_size;
        }
    };

    template <typename Scalar>
    void traverse(const Bvh<Scalar> &bvh, size_t node_index = 0, size_t depth = 0)
    {
        // Ensure there is at least one node to traverse
        if (bvh.node_count == 0)
            return;

        const auto *nodes = bvh.nodes.get();
        const auto *primitive_indices = bvh.primitive_indices.get();
        const auto &node = nodes[node_index];

        // Print the current node's information
        printf("%*sNode %ld:\n", static_cast<int>(depth * 2), "", node_index);
        auto bbox = node.bounding_box_proxy().to_bounding_box();
        printf("%*s  BoundingBox: [%f, %f, %f] to [%f, %f, %f]\n",
               static_cast<int>(depth * 2), "",
               bbox.min[0], bbox.min[1], bbox.min[2],
               bbox.max[0], bbox.max[1], bbox.max[2]);

        if (node.is_leaf())
        {
            // Leaf node: print the primitive indices
            printf("%*s  Primitives (index): ", static_cast<int>(depth * 2), "");
            for (size_t i = 0; i < node.primitive_count; ++i)
            {
                printf("%ld ", primitive_indices[node.first_child_or_primitive + i]);
            }
            printf("\n");
        }
        else
        {
            // Internal node: traverse children
            size_t left_child_index = node.first_child_or_primitive;
            size_t right_child_index = left_child_index + 1;
            traverse<Scalar>(bvh, left_child_index, depth + 1);
            traverse<Scalar>(bvh, right_child_index, depth + 1);
        }
    }

} // namespace bvh

#endif
