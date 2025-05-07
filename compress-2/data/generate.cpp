#include <iostream>
#include <random>
#include <bvh/bvh.hpp>
#include <bvh/triangle.hpp>
#include <bvh/sweep_sah_builder.hpp>
#include <bvh/single_ray_traverser.hpp>
#include <bvh/primitive_intersectors.hpp>
#include <happly.h>
#include "../../include/datatypes.h"
#include "gentypes.h"

typedef bvh::Bvh<float> bvh_t;
typedef bvh::Triangle<float> bvh_trig_t;
typedef bvh::Ray<float> bvh_ray_t;
typedef bvh::Vector<float, 3> vector_t;
typedef bvh::SweepSahBuilder<bvh_t> builder_t;
typedef bvh::SingleRayTraverser<bvh_t> traverser_t;
typedef bvh::ClosestPrimitiveIntersector<bvh_t, bvh_trig_t> primitive_intersector_t;

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cout << "usage: " << argv[0] << " MODEL RAY" << std::endl;
        exit(EXIT_FAILURE);
    }

    char* model_fn = argv[1];
    char* ray_fn = argv[2];

    happly::PLYData ply_data(model_fn);
    std::vector<std::array<double, 3>> v_pos = ply_data.getVertexPositions();
    std::vector<std::vector<size_t>> f_idx = ply_data.getFaceIndices<size_t>();

    std::vector<bvh_trig_t> bvh_trig_;
    for (auto &face : f_idx) {
        bvh_trig_.emplace_back(
            vector_t((float)v_pos[face[0]][0], (float)v_pos[face[0]][1], (float)v_pos[face[0]][2]),
            vector_t((float)v_pos[face[1]][0], (float)v_pos[face[1]][1], (float)v_pos[face[1]][2]),
            vector_t((float)v_pos[face[2]][0], (float)v_pos[face[2]][1], (float)v_pos[face[2]][2])
        );
    }

    auto [bboxes, centers] = bvh::compute_bounding_boxes_and_centers(bvh_trig_.data(), bvh_trig_.size());
    auto global_bbox = bvh::compute_bounding_boxes_union(bboxes.get(), bvh_trig_.size());
    std::cout << "global_bbox = ("
              << global_bbox.min[0] << ", " << global_bbox.min[1] << ", " << global_bbox.min[2] << "), ("
              << global_bbox.max[0] << ", " << global_bbox.max[1] << ", " << global_bbox.max[2] << ")" << std::endl;

    bvh_t bvh;
    builder_t builder(bvh);
    builder.max_leaf_size = MAX_TRIGS_PER_NODE;
    builder.build(global_bbox, bboxes.get(), centers.get(), bvh_trig_.size());
    std::unique_ptr<bvh_trig_t[]> permuted_bvh_trig_ = bvh::permute_primitives(bvh_trig_.data(),
                                                                               bvh.primitive_indices.get(),
                                                                               bvh_trig_.size());

    traverser_t traverser(bvh);
    primitive_intersector_t primitive_intersector(bvh, bvh_trig_.data());

    std::vector<ray_t> ray_;
    std::vector<result_t> result_;
    {
        std::ifstream ray_fs(ray_fn);
        for (float r[7]; ray_fs.read((char*)r, 7 * sizeof(float)); ) {
            ray_t ray = {
                .origin_x = r[0],
                .origin_y = r[1],
                .origin_z = r[2],
                .dir_x    = r[3],
                .dir_y    = r[4],
                .dir_z    = r[5],
                .tmin     = 0.0f,
                .tmax     = r[6]
            };
            ray_.push_back(ray);

            bvh_ray_t bvh_ray;
            bvh_ray.origin[0]    = ray.origin_x;
            bvh_ray.origin[1]    = ray.origin_y;
            bvh_ray.origin[2]    = ray.origin_z;
            bvh_ray.direction[0] = ray.dir_x;
            bvh_ray.direction[1] = ray.dir_y;
            bvh_ray.direction[2] = ray.dir_z;
            bvh_ray.tmin         = ray.tmin;
            bvh_ray.tmax         = ray.tmax;

            auto bvh_result = traverser.traverse(bvh_ray, primitive_intersector);
            if (bvh_result.has_value()) {
                result_t result = {
                    .intersected = true,
                    .t = bvh_result->intersection.t,
                    .u = bvh_result->intersection.u,
                    .v = bvh_result->intersection.v
                };
                result_.push_back(result);
            } else {
                result_t result = {
                    .intersected = false,
                };
                result_.push_back(result);
            }
        }
    }

    gen_ref_bbox_t gen_ref_bbox = {
        .x_min = bvh.nodes[0].bounds[0],
        .y_min = bvh.nodes[0].bounds[2],
        .z_min = bvh.nodes[0].bounds[4]
    };
    
    std::vector<std::array<uint8_t, 3>> exp(bvh.node_count);
    std::vector<std::array<float, 6>> bounds(bvh.node_count);
    std::vector<std::array<uint8_t, 6>> bounds_quant(bvh.node_count);
    std::copy_n(bvh.nodes[0].bounds, 6, bounds[0].begin());

    std::queue<size_t> queue({0});
    while (!queue.empty()) {
        size_t curr_idx = queue.front();
        auto& curr_node = bvh.nodes[curr_idx];
        queue.pop();

        if (curr_node.is_leaf())
            continue;

        float extent[3] = {
            bounds[curr_idx][1] - bounds[curr_idx][0],
            bounds[curr_idx][3] - bounds[curr_idx][2],
            bounds[curr_idx][5] - bounds[curr_idx][4]
        };

        size_t left_idx = curr_node.first_child_or_primitive;
        size_t right_idx = left_idx + 1;

        for (auto idx : std::array<size_t, 2>{left_idx, right_idx}) {
            auto& node = bvh.nodes[idx];
            for (int i = 0; i < 3; i++) {
                std::array<float, 3> exp_fp32 = {};
                exp[idx][i] = static_cast<uint8_t>(std::ceil(std::log2((extent[i] == 0.0f ? 1.0f : extent[i]) / 255.0f)) + 127.0f);
                exp_fp32[i] = std::pow(2.0f, exp[idx][i] - 127.0f);

                float bound_quant_min_fp32 = std::floor((node.bounds[i * 2] - bounds[curr_idx][i * 2]) / exp_fp32[i]);
                float bound_quant_max_fp32 = std::ceil((node.bounds[i * 2 + 1] - bounds[curr_idx][i * 2]) / exp_fp32[i]);
                assert(0.0f <= bound_quant_min_fp32 && bound_quant_min_fp32 <= 255.0f);
                assert(0.0f <= bound_quant_max_fp32 && bound_quant_max_fp32 <= 255.0f);

                bounds[idx][i * 2] = bounds[curr_idx][i * 2] + bound_quant_min_fp32 * exp_fp32[i];
                bounds[idx][i * 2 + 1] = bounds[curr_idx][i * 2] + bound_quant_max_fp32 * exp_fp32[i];
                assert(bounds[idx][i * 2] <= node.bounds[i * 2]);
                assert(bounds[idx][i * 2 + 1] >= node.bounds[i * 2 + 1]);

                bounds_quant[idx][i * 2] = static_cast<uint8_t>(bound_quant_min_fp32);
                bounds_quant[idx][i * 2 + 1] = static_cast<uint8_t>(bound_quant_max_fp32);
            }
        }

        queue.emplace(left_idx);
        queue.emplace(right_idx);
    }

    int gen_nbp_size = (int)bvh.node_count / 2;
    auto gen_nbp_ = std::make_unique<gen_nbp_t[]>(gen_nbp_size);
    assert(bvh.node_count % 2 == 1);
    for (int i = 1; i < (int)bvh.node_count; i += 2) {
        gen_nbp_t gen_nbp = {
            .left_node_num_trigs  = bvh.nodes[i    ].primitive_count,
            .left_node_child_idx  = bvh.nodes[i    ].first_child_or_primitive,
            .right_node_num_trigs = bvh.nodes[i + 1].primitive_count,
            .right_node_child_idx = bvh.nodes[i + 1].first_child_or_primitive,
            .exp_x                = exp[i][0],
            .exp_y                = exp[i][1],
            .exp_z                = exp[i][2],
            .left_bbox_x_min      = bounds_quant[i    ][0],
            .left_bbox_x_max      = bounds_quant[i    ][1],
            .left_bbox_y_min      = bounds_quant[i    ][2],
            .left_bbox_y_max      = bounds_quant[i    ][3],
            .left_bbox_z_min      = bounds_quant[i    ][4],
            .left_bbox_z_max      = bounds_quant[i    ][5],
            .right_bbox_x_min     = bounds_quant[i + 1][0],
            .right_bbox_x_max     = bounds_quant[i + 1][1],
            .right_bbox_y_min     = bounds_quant[i + 1][2],
            .right_bbox_y_max     = bounds_quant[i + 1][3],
            .right_bbox_z_min     = bounds_quant[i + 1][4],
            .right_bbox_z_max     = bounds_quant[i + 1][5],
        };

        if (gen_nbp.left_node_num_trigs == 0) {
            assert(gen_nbp.left_node_child_idx % 2 == 1);
            gen_nbp.left_node_child_idx /= 2;
        }
        if (gen_nbp.right_node_num_trigs == 0) {
            assert(gen_nbp.right_node_child_idx % 2 == 1);
            gen_nbp.right_node_child_idx /= 2;
        }

        gen_nbp_[i / 2] = gen_nbp;
    }

    int trig_size = (int)bvh_trig_.size();
    auto trig_ = std::make_unique<trig_t[]>(trig_size);
    for (int i = 0; i < trig_size; i++) {
        trig_[i] = {
            .p0_x = permuted_bvh_trig_[i].p0[0],
            .p0_y = permuted_bvh_trig_[i].p0[1],
            .p0_z = permuted_bvh_trig_[i].p0[2],
            .e1_x = permuted_bvh_trig_[i].e1[0],
            .e1_y = permuted_bvh_trig_[i].e1[1],
            .e1_z = permuted_bvh_trig_[i].e1[2],
            .e2_x = permuted_bvh_trig_[i].e2[0],
            .e2_y = permuted_bvh_trig_[i].e2[1],
            .e2_z = permuted_bvh_trig_[i].e2[2]
        };
    }

    std::ofstream gen_ref_bbox_fs("gen_ref_bbox.bin", std::ios::binary);
    gen_ref_bbox_fs.write((char*)(&gen_ref_bbox), sizeof(gen_ref_bbox_t));

    std::ofstream gen_nbp_fs("gen_nbp.bin", std::ios::binary);
    for (int i = 0; i < gen_nbp_size; i++)
        gen_nbp_fs.write((char*)(&gen_nbp_[i]), sizeof(gen_nbp_t));

    std::ofstream trig_fs("trig.bin", std::ios::binary);
    for (int i = 0; i < trig_size; i++)
        trig_fs.write((char*)(&trig_[i]), sizeof(trig_t));

    std::ofstream ray_fs("ray.bin", std::ios::binary);
    for (auto &ray : ray_)
        ray_fs.write((char*)(&ray), sizeof(ray_t));

    assert(result_.size() == ray_.size());
    std::ofstream result_fs("result.bin", std::ios::binary);
    for (auto &result : result_)
        result_fs.write((char*)(&result), sizeof(result_t));

    return 0;
}
