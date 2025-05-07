#include "build.hpp"
#include "traverse.hpp"
#include <bvh/single_ray_traverser.hpp>
#include <bvh/primitive_intersectors.hpp>

using namespace bvh_quantize;
typedef bvh::SingleRayTraverser<bvh_t> traverser_t;
typedef bvh::ClosestPrimitiveIntersector<bvh_t, trig_t> primitive_intersector_t;

int main(int argc, char *argv[]) {
    arg_t arg = parse_arg(argc, argv);

    std::cout << "loading..." << std::endl;
    std::vector<trig_t> trigs = load_trigs(arg.model_file);

    std::cout << "building..." << std::endl;
    bvh_t bvh = build_bvh(trigs);

    std::cout << "clustering..." << std::endl;
    int_bvh_t int_bvh = build_int_bvh(arg.t_trv_int, arg.t_switch, arg.t_ist, trigs, bvh);

    std::cout << "visualizing..." << std::endl;
    gen_tree_visualization_and_report_stack_size_requirement(int_bvh);
    gen_scene_visualization(bvh, int_bvh);

    if (arg.ray_file == nullptr)
        return 0;

    std::cout << "traversing..." << std::endl;
    intmax_t correct_rays = 0;
    intmax_t total_rays = 0;
    traverser_t full_traverser(bvh);
    primitive_intersector_t primitive_intersector(bvh, trigs.data());
    traverser_t::Statistics full_statistics;
    statistics_t int_statistics;
    std::ifstream ray_fs(arg.ray_file);
    for (float r[7]; ray_fs.read((char*)r, 7 * sizeof(float)); total_rays++) {
        ray_t ray(
            vector_t(r[0], r[1], r[2]),
            vector_t(r[3], r[4], r[5]),
            0.f,
            r[6]
        );
        auto full_result = full_traverser.traverse(ray, primitive_intersector, full_statistics);
        auto int_result = int_traverse(int_bvh, ray, int_statistics);

        if (full_result.has_value()) {
            if (int_result.has_value() &&
                int_result->t == full_result->intersection.t &&
                int_result->u == full_result->intersection.u &&
                int_result->v == full_result->intersection.v)
                correct_rays++;
        } else if (!int_result.has_value()) {
                correct_rays++;
        }
    }

    std::cout << "(vanilla)" << std::endl;
    std::cout << "  traversal_steps: " << full_statistics.traversal_steps << std::endl;
    std::cout << "  both_intersected: " << full_statistics.both_intersected << std::endl;
    std::cout << "  intersections_a: " << full_statistics.intersections_a << std::endl;
    std::cout << "  intersections_b: " << full_statistics.intersections_b << std::endl;
    std::cout << "  finalize: " << full_statistics.finalize << std::endl;

    std::cout << "(quantized)" << std::endl;
    std::cout << "  intersect_bbox: " << int_statistics.intersect_bbox << std::endl;
    std::cout << "  push_cluster: " << int_statistics.push_cluster << std::endl;
    std::cout << "  recompute_qymax: " << int_statistics.recompute_qymax << std::endl;
    std::cout << "  traversal_steps: " << int_statistics.traversal_steps << std::endl;
    std::cout << "  both_intersected: " << int_statistics.both_intersected << std::endl;
    std::cout << "  intersections_a: " << int_statistics.bvh_statistics.intersections_a << std::endl;
    std::cout << "  intersections_b: " << int_statistics.bvh_statistics.intersections_b << std::endl;
    std::cout << "  finalize: " << int_statistics.finalize << std::endl;

    std::cout << "total_rays: " << total_rays << std::endl;
    std::cout << "correct_rays: " << correct_rays << std::endl;
}