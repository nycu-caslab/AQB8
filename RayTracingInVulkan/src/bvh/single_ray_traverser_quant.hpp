#ifndef BVH_SINGLE_RAY_TRAVERSAL_QUANT_HPP
#define BVH_SINGLE_RAY_TRAVERSAL_QUANT_HPP

#include <cassert>

#include "bvh/bvh.hpp"
#include "bvh/ray.hpp"
#include "bvh/node_intersectors.hpp"
#include "bvh/utilities.hpp"

namespace bvh
{

    /// Single ray traversal algorithm, using the provided ray-node intersector.
    template <typename Bvh, size_t StackSize = 1024, typename NodeIntersector = FastNodeIntersector<Bvh>>
    class SingleRayTraverserQuant
    {
    public:
        static constexpr size_t stack_size = StackSize;

    private:
        using Scalar = typename Bvh::ScalarType;

        struct Stack
        {
            using Element = std::tuple<typename Bvh::IndexType, std::array<float, 3>>;

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
                const uint32_t &primitive_index,
                const uint32_t &primitive_count,
                Ray<Scalar> &ray,
                std::optional<typename PrimitiveIntersector::Result> &best_hit,
                PrimitiveIntersector &primitive_intersector,
                Statistics &statistics) const
        {
            assert(primitive_count);
            size_t begin = primitive_index;
            size_t end = begin + primitive_count;
            for (size_t i = begin; i < end; ++i)
            {
                if (auto hit = primitive_intersector.intersect(i, ray, statistics))
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
            NodeIntersector node_intersector(ray);

            // This traversal loop is eager, because it immediately processes leaves instead of pushing them on the stack.
            // This is generally beneficial for performance because intersections will likely be found which will
            // allow to cull more subtrees with the ray-box test of the traversal loop.
            Stack stack;
            Stack stack_prim;

            std::array<float, 3> curr_min_bounds = {
                bvh.nodes[0].bounds[0],
                bvh.nodes[0].bounds[2],
                bvh.nodes[0].bounds[4],
            };

            auto *curr_node = &bvh.nodes_v2[0];
            uint32_t primitive_count = 6;

            while (true)
            {
                statistics.traversal_steps++;

                bool is_leaf[6];
                uint32_t child_index[6];
                uint32_t child_primitive_count[6];

                std::array<std::array<float, 6>, 6> bounds = {};
                std::vector<std::pair<typename Bvh::Compress_node_v2 *, float *>> child_bounds_pair;

                for (int i = 0; i < primitive_count; i++)
                {
                    is_leaf[i] = (bool)((curr_node->data[i] >> 31) & 0x1);       // bit 31
                    child_primitive_count[i] = (curr_node->data[i] >> 28) & 0x7; // bits 28–30
                    child_index[i] = curr_node->data[i] & 0x0FFFFFFF;            // bits 0–27 (28 bits)

                    auto *child_node = &bvh.nodes_v2[child_index[i]];
                    child_bounds_pair.emplace_back(child_node, bounds[i].data());
                }

                for (int i = 0; i < primitive_count; i++)
                {
                    for (int j = 0; j < 3; j++)
                    {
                        child_bounds_pair[i].second[j * 2] = curr_min_bounds[j] + ldexpf(curr_node->bounds_quant[i][j * 2], curr_node->exp[j] - 8);
                        child_bounds_pair[i].second[j * 2 + 1] = curr_min_bounds[j] + ldexpf(curr_node->bounds_quant[i][j * 2 + 1], curr_node->exp[j] - 8);
                    }
                }

                std::array<std::array<float, 3>, 6> min_bounds;
                for (int i = 0; i < primitive_count; i++)
                {
                    min_bounds[i][0] = bounds[i][0];
                    min_bounds[i][1] = bounds[i][2];
                    min_bounds[i][2] = bounds[i][4];
                }

                std::vector<std::pair<float, int>> sorted_nodes;

                for (int i = 0; i < primitive_count; i++)
                {
                    auto distance = node_intersector.intersect(bounds[i].data(), ray);

                    if (distance.first <= distance.second)
                    {
                        if (is_leaf[i])
                        {
                            if (intersect_leaf(child_index[i], child_primitive_count[i], ray, best_hit, primitive_intersector, statistics) &&
                                primitive_intersector.any_hit)
                                break;
                        }
                        else
                        {
                            sorted_nodes.emplace_back(distance.first, i);
                        }
                    }
                }

                std::sort(sorted_nodes.begin(), sorted_nodes.end());

                if (sorted_nodes.size() > 1)
                    statistics.both_intersected++;

                for (size_t i = 1; i < sorted_nodes.size(); i++)
                {
                    int idx = sorted_nodes[i].second;
                    stack.push({child_index[idx], min_bounds[idx]});
                    stack_prim.push({child_primitive_count[idx], min_bounds[idx]});
                }

                if (sorted_nodes.size() > 0)
                {
                    int idx = sorted_nodes[0].second;
                    curr_node = &bvh.nodes_v2[child_index[idx]];
                    curr_min_bounds = min_bounds[idx];
                    primitive_count = child_primitive_count[idx];
                }
                else
                {
                    if (stack.empty() || stack_prim.empty())
                        break;
                    auto [t1, t2] = stack.pop();
                    curr_node = &bvh.nodes_v2[t1];
                    curr_min_bounds = t2;

                    auto [t3, t4] = stack_prim.pop();
                    primitive_count = t3;
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

        SingleRayTraverserQuant(const Bvh &bvh)
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
