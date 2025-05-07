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
    template <typename Bvh, size_t StackSize = 64, typename NodeIntersector = FastNodeIntersector<Bvh>>
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
            std::array<float, 3> curr_min_bounds = {
                bvh.nodes[0].bounds[0],
                bvh.nodes[0].bounds[2],
                bvh.nodes[0].bounds[4],
            };

            auto *curr_node = &bvh.nodes_v2[0];

            while (true)
            {
                statistics.traversal_steps++;
                uint32_t left_child_index = curr_node->left_child_data & 0x1FFFFFFF;
                uint32_t right_child_index = curr_node->right_child_data & 0x1FFFFFFF;
                uint32_t left_child_primitive_count = (curr_node->left_child_data >> 29) & 0x7;
                uint32_t right_child_primitive_count = (curr_node->right_child_data >> 29) & 0x7;

                auto *left_child = &bvh.nodes_v2[left_child_index];
                auto *right_child = &bvh.nodes_v2[right_child_index];

                std::array<float, 6> left_bounds = {};
                std::array<float, 6> right_bounds = {};
                std::vector<std::pair<typename Bvh::Compress_node_v2 *, float *>> child_bounds_pair = {
                    {left_child, left_bounds.data()},
                    {right_child, right_bounds.data()}};

                for (int i = 0; i < 3; i++)
                {
                    child_bounds_pair[0].second[i * 2] = curr_min_bounds[i] + ldexpf(curr_node->left_bounds_quant[i * 2], curr_node->exp[i] - 8);
                    child_bounds_pair[0].second[i * 2 + 1] = curr_min_bounds[i] + ldexpf(curr_node->left_bounds_quant[i * 2 + 1], curr_node->exp[i] - 8);
                }

                for (int i = 0; i < 3; i++)
                {
                    child_bounds_pair[1].second[i * 2] = curr_min_bounds[i] + ldexpf(curr_node->right_bounds_quant[i * 2], curr_node->exp[i] - 8);
                    child_bounds_pair[1].second[i * 2 + 1] = curr_min_bounds[i] + ldexpf(curr_node->right_bounds_quant[i * 2 + 1], curr_node->exp[i] - 8);
                }

                std::array<float, 3> left_min_bounds = {
                    left_bounds[0],
                    left_bounds[2],
                    left_bounds[4]};
                std::array<float, 3> right_min_bounds = {
                    right_bounds[0],
                    right_bounds[2],
                    right_bounds[4]};

                // std::array<int8_t, 3> left_exp = {
                //     curr_node->left_exp[0],
                //     curr_node->left_exp[1],
                //     curr_node->left_exp[2]};
                // std::array<int8_t, 3> right_exp = {
                //     curr_node->right_exp[0],
                //     curr_node->right_exp[1],
                //     curr_node->right_exp[2]};

                auto distance_left = node_intersector.intersect(left_bounds.data(), ray);
                auto distance_right = node_intersector.intersect(right_bounds.data(), ray);

                if (distance_left.first <= distance_left.second)
                {
                    if (bvh_unlikely(left_child_primitive_count))
                    {
                        if (intersect_leaf(left_child_index, left_child_primitive_count, ray, best_hit, primitive_intersector, statistics) &&
                            primitive_intersector.any_hit)
                            break;
                        left_child = nullptr;
                    }
                }
                else
                    left_child = nullptr;

                if (distance_right.first <= distance_right.second)
                {
                    if (bvh_unlikely(right_child_primitive_count))
                    {
                        if (intersect_leaf(right_child_index, right_child_primitive_count, ray, best_hit, primitive_intersector, statistics) &&
                            primitive_intersector.any_hit)
                            break;
                        right_child = nullptr;
                    }
                }
                else
                    right_child = nullptr;

                if (left_child)
                {
                    if (right_child)
                    {
                        statistics.both_intersected++;
                        if (distance_left.first > distance_right.first)
                        {
                            stack.push({left_child_index, left_min_bounds});
                            curr_node = right_child;
                            curr_min_bounds = right_min_bounds;
                        }
                        else
                        {
                            stack.push({right_child_index, right_min_bounds});
                            curr_node = left_child;
                            curr_min_bounds = left_min_bounds;
                        }
                    }
                    else
                    {
                        curr_node = left_child;
                        curr_min_bounds = left_min_bounds;
                    }
                }
                else if (right_child)
                {
                    curr_node = right_child;
                    curr_min_bounds = right_min_bounds;
                }
                else
                {
                    if (stack.empty())
                        break;
                    auto [t1, t2] = stack.pop();
                    curr_node = &bvh.nodes_v2[t1];
                    curr_min_bounds = t2;
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
