#ifndef BVH_SINGLE_RAY_TRAVERSAL_HPP
#define BVH_SINGLE_RAY_TRAVERSAL_HPP

#include <cassert>

#include "bvh/bvh.hpp"
#include "bvh/ray.hpp"
#include "bvh/node_intersectors.hpp"
#include "bvh/utilities.hpp"

namespace bvh
{

    /// Single ray traversal algorithm, using the provided ray-node intersector.
    template <typename Bvh, size_t StackSize = 64, typename NodeIntersector = FastNodeIntersector<Bvh>>
    class SingleRayTraverser
    {
    public:
        static constexpr size_t stack_size = StackSize;

    private:
        using Scalar = typename Bvh::ScalarType;

        struct Stack
        {
            using Element = typename Bvh::IndexType;

            Element elements[stack_size];
            size_t size = 0;

            void push(const Element &t)
            {
                assert(size < stack_size);
                elements[size++] = t;
            }

            Element pop()
            {
                assert(!empty());
                return elements[--size];
            }

            bool empty() const { return size == 0; }
        };

        template <typename PrimitiveIntersector, typename Statistics>
        bvh_always_inline
            std::optional<typename PrimitiveIntersector::Result> &
            intersect_leaf(
                const typename Bvh::Node &node,
                Ray<Scalar> &ray,
                std::optional<typename PrimitiveIntersector::Result> &best_hit,
                PrimitiveIntersector &primitive_intersector,
                Statistics &statistics) const
        {
            assert(node.is_leaf());
            size_t primitive_index = node.first_child_or_primitive;
            size_t primitive_count = node.primitive_count;

            for (size_t i = 0; i < primitive_count; ++i)
            {
                if (auto hit = primitive_intersector.intersect(primitive_index + i, ray, statistics))
                {
                    best_hit = hit;
                    if (primitive_intersector.any_hit)
                        return best_hit;
                    ray.tmax = hit->distance();
                }
            }

            return best_hit;
        }

        template <typename PrimitiveIntersector, typename Statistics>
        bvh_always_inline
            std::optional<typename PrimitiveIntersector::Result>
            intersect(Ray<Scalar> ray, PrimitiveIntersector &primitive_intersector, Statistics &statistics) const
        {
            auto best_hit = std::optional<typename PrimitiveIntersector::Result>(std::nullopt);

            // If the root is a leaf, intersect it and return
            if (bvh_unlikely(bvh.nodes[0].is_leaf()))
                return intersect_leaf(bvh.nodes[0], ray, best_hit, primitive_intersector, statistics);

            NodeIntersector node_intersector(ray);

            // Traversal stack
            Stack stack;
            stack.push(0);

            while (!stack.empty())
            {
                size_t node_idx = stack.pop();
                const auto &node = bvh.nodes[node_idx];

                if (bvh_unlikely(node.is_leaf()))
                {
                    if (intersect_leaf(node, ray, best_hit, primitive_intersector, statistics) &&
                        primitive_intersector.any_hit)
                    {
                        statistics.finalize++;
                        return best_hit; // Early exit
                    }

                    continue;
                }

                statistics.traversal_steps++;

                // Collect valid children with distances
                std::vector<std::pair<float, size_t>> children_distance;
                size_t child_count = node.primitive_count;

                for (size_t i = 0; i < child_count; ++i)
                {
                    auto child_idx = node.first_child_or_primitive + i;
                    const auto &child = bvh.nodes[child_idx];
                    auto distance = node_intersector.intersect(child, ray);

                    if (distance.first <= distance.second)
                    {
                        children_distance.emplace_back(distance.first, child_idx);
                    }
                }

                if (children_distance.size() > 1)
                {
                    statistics.both_intersected++;
                }

                // Sort valid children by distance
                std::sort(children_distance.begin(), children_distance.end());

                // Push valid children onto the stack in reverse order
                for (auto it = children_distance.rbegin(); it != children_distance.rend(); ++it)
                {
                    stack.push(it->second);
                }
            }

            if (best_hit.has_value())
                statistics.finalize++;
            return best_hit;
        }

        const Bvh &bvh;

    public:
        /// Statistics collected during traversal.
        struct Statistics
        {
            size_t traversal_steps = 0;
            size_t both_intersected = 0;
            size_t intersections_a = 0;
            size_t intersections_b = 0;
            size_t finalize = 0;
        };

        SingleRayTraverser(const Bvh &bvh)
            : bvh(bvh)
        {
        }

        /// Intersects the BVH with the given ray and intersector.
        template <typename PrimitiveIntersector>
        bvh_always_inline
            std::optional<typename PrimitiveIntersector::Result>
            traverse(const Ray<Scalar> &ray, PrimitiveIntersector &intersector) const
        {
            struct
            {
                struct Empty
                {
                    Empty &operator++(int) { return *this; }
                    Empty &operator++() { return *this; }
                    Empty &operator+=(size_t) { return *this; }
                } traversal_steps, both_intersected, intersections_a, intersections_b, finalize;
            } statistics;
            return intersect(ray, intersector, statistics);
        }

        /// Intersects the BVH with the given ray and intersector.
        /// Record statistics on the number of traversal and intersection steps.
        template <typename PrimitiveIntersector>
        bvh_always_inline
            std::optional<typename PrimitiveIntersector::Result>
            traverse(const Ray<Scalar> &ray, PrimitiveIntersector &primitive_intersector, Statistics &statistics) const
        {
            return intersect(ray, primitive_intersector, statistics);
        }
    };

} // namespace bvh

#endif
