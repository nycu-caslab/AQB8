#include <iostream>
#include <random>
#include <bvh/bvh.hpp>
#include <bvh/triangle.hpp>
#include <bvh/sweep_sah_builder.hpp>
#include <bvh/single_ray_traverser.hpp>
#include <bvh/primitive_intersectors.hpp>
#include <happly.h>
#include <build.hpp>
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
    if (argc != 6) {
        std::cout << "usage: " << argv[0] << " MODEL_FILE RAY_FILE T_TRV_INT T_SWITCH T_IST" << std::endl;
        exit(EXIT_FAILURE);
    }

    char* model_fn = argv[1];
    char* ray_fn = argv[2];
    float t_trv_int = std::stof(argv[3]);
    float t_switch = std::stof(argv[4]);
    float t_ist = std::stof(argv[5]);

    std::cout << "MODEL_FILE = " << model_fn << std::endl;
    std::cout << "RAY_FILE = " << ray_fn << std::endl;
    std::cout << "T_TRV_INT = " << t_trv_int << std::endl;
    std::cout << "T_SWITCH = " << t_switch << std::endl;
    std::cout << "T_IST = " << t_ist << std::endl;

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

    traverser_t traverser(bvh);
    primitive_intersector_t primitive_intersector(bvh, bvh_trig_.data());

    std::vector<ray_t> ray_;
    std::vector<result_t> result_;
    {
        std::ifstream ray_fs(ray_fn);
        for (float r[7]; ray_fs.read((char*)r, 7 * sizeof(float)); ) {
            float dir_length = r[3] * r[3] + r[4] * r[4] + r[5] * r[5];
            dir_length = std::sqrt(dir_length);
            ray_t ray = {
                .origin_x = r[0],
                .origin_y = r[1],
                .origin_z = r[2],
                .dir_x    = r[3] / dir_length,
                .dir_y    = r[4] / dir_length,
                .dir_z    = r[5] / dir_length,
                .tmin     = 0.0f,
                .tmax     = r[6] * dir_length
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

    bvh_quantize::int_bvh_t int_bvh = bvh_quantize::build_int_bvh(t_trv_int, t_switch, t_ist, bvh_trig_, bvh);

    int gen_nbp_size = (int)bvh.node_count / 2;
    auto gen_nbp_ = std::make_unique<gen_nbp_t[]>(gen_nbp_size);
    assert(bvh.node_count % 2 == 1);
    for (int i = 0; i < (int)bvh.node_count - 1; i += 2) {
        uint32_t left_node_field_a  = (int_bvh.nodes[i].data >> 15);
        uint32_t left_node_field_b  = ((int_bvh.nodes[i].data >> 12) & 0b0111);
        uint32_t left_node_field_c  = (int_bvh.nodes[i].data & 0xfff);
        uint32_t right_node_field_a = (int_bvh.nodes[i + 1].data >> 15);
        uint32_t right_node_field_b = ((int_bvh.nodes[i + 1].data >> 12) & 0b0111);
        uint32_t right_node_field_c = (int_bvh.nodes[i + 1].data & 0xfff);
        gen_nbp_t gen_nbp = {
            .left_node_field_a  = left_node_field_a,
            .left_node_field_b  = left_node_field_b,
            .left_node_field_c  = left_node_field_c,
            .right_node_field_a = right_node_field_a,
            .right_node_field_b = right_node_field_b,
            .right_node_field_c = right_node_field_c,
            .left_bbox_x_min    = int_bvh.nodes[i    ].bounds[0],
            .left_bbox_x_max    = int_bvh.nodes[i    ].bounds[1],
            .left_bbox_y_min    = int_bvh.nodes[i    ].bounds[2],
            .left_bbox_y_max    = int_bvh.nodes[i    ].bounds[3],
            .left_bbox_z_min    = int_bvh.nodes[i    ].bounds[4],
            .left_bbox_z_max    = int_bvh.nodes[i    ].bounds[5],
            .right_bbox_x_min   = int_bvh.nodes[i + 1].bounds[0],
            .right_bbox_x_max   = int_bvh.nodes[i + 1].bounds[1],
            .right_bbox_y_min   = int_bvh.nodes[i + 1].bounds[2],
            .right_bbox_y_max   = int_bvh.nodes[i + 1].bounds[3],
            .right_bbox_z_min   = int_bvh.nodes[i + 1].bounds[4],
            .right_bbox_z_max   = int_bvh.nodes[i + 1].bounds[5],
        };

        if (gen_nbp.left_node_field_a == 1 && gen_nbp.left_node_field_b == 0) {
            assert(gen_nbp.left_node_field_c % 2 == 0);
            gen_nbp.left_node_field_c /= 2;
        }
        if (gen_nbp.right_node_field_a == 1 && gen_nbp.right_node_field_b == 0) {
            assert(gen_nbp.right_node_field_c % 2 == 0);
            gen_nbp.right_node_field_c /= 2;
        }

        gen_nbp_[i / 2] = gen_nbp;
    }

    int gen_cluster_size = int_bvh.num_clusters;
    auto gen_cluster_ = std::make_unique<gen_cluster_t[]>(gen_cluster_size);
    for (int i = 0; i < gen_cluster_size; i++) {
        gen_cluster_[i] = {
            .x_min         = int_bvh.clusters[i].ref_bounds[0],
            .x_max         = int_bvh.clusters[i].ref_bounds[1],
            .y_min         = int_bvh.clusters[i].ref_bounds[2],
            .y_max         = int_bvh.clusters[i].ref_bounds[3],
            .z_min         = int_bvh.clusters[i].ref_bounds[4],
            .z_max         = int_bvh.clusters[i].ref_bounds[5],
            .inv_sx_inv_sw = int_bvh.clusters[i].inv_sx_inv_sw,
            .nbp_offset    = int_bvh.clusters[i].node_offset / 2,
            .trig_offset   = int_bvh.clusters[i].trig_offset
        };
    }

    int trig_size = (int)bvh_trig_.size();
    auto trig_ = std::make_unique<trig_t[]>(trig_size);
    for (int i = 0; i < trig_size; i++) {
        trig_[i] = {
            .p0_x = int_bvh.trigs[i].p0[0],
            .p0_y = int_bvh.trigs[i].p0[1],
            .p0_z = int_bvh.trigs[i].p0[2],
            .e1_x = int_bvh.trigs[i].e1[0],
            .e1_y = int_bvh.trigs[i].e1[1],
            .e1_z = int_bvh.trigs[i].e1[2],
            .e2_x = int_bvh.trigs[i].e2[0],
            .e2_y = int_bvh.trigs[i].e2[1],
            .e2_z = int_bvh.trigs[i].e2[2]
        };
    }

    std::ofstream gen_nbp_fs("gen_nbp.bin", std::ios::binary);
    for (int i = 0; i < gen_nbp_size; i++)
        gen_nbp_fs.write((char*)(&gen_nbp_[i]), sizeof(gen_nbp_t));

    std::ofstream gen_cluster_fs("gen_cluster.bin", std::ios::binary);
    for (int i = 0; i < gen_cluster_size; i++)
        gen_cluster_fs.write((char*)(&gen_cluster_[i]), sizeof(gen_cluster_t));

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
