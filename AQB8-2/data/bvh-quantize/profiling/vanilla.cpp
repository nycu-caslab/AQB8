#include "../src/build.hpp"
#include "../src/traverse.hpp"
#include <bvh/single_ray_traverser.hpp>
#include <bvh/primitive_intersectors.hpp>
#include <unistd.h>

typedef bvh::SingleRayTraverser<bvh_t> traverser_t;
typedef bvh::ClosestPrimitiveIntersector<bvh_t, trig_t> primitive_intersector_t;

// prevent gcc from optimizing out result
intersection_t full_result __attribute__ ((used));

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "usage: " << argv[0] << " SCENE_NAME T_TRV_INT T_SWITCH" << std::endl;
        exit(EXIT_FAILURE);
    }

    // perf control fifo
    char* ctl_fd_str = getenv("CTL_FD");
    assert(ctl_fd_str != nullptr);
    char* ack_fd_str = getenv("ACK_FD");
    assert(ack_fd_str != nullptr);
    int ctl_fd = std::stoi(ctl_fd_str);
    int ack_fd = std::stoi(ack_fd_str);
    char ack[5];

    std::string model_file = std::string("../data/scene/") + std::string(argv[1]) + ".ply";
    std::string ray_file = std::string("../data/scene/") + std::string(argv[1]) + ".ray";

    arg_t arg = {
        .model_file = (char*)model_file.c_str(),
        .t_trv_int = std::stof(argv[2]),
        .t_switch = std::stof(argv[3]),
        .t_ist = 1,
        .ray_file = (char*)ray_file.c_str()
    };

    std::cout << "model_file = " << arg.model_file << std::endl;
    std::cout << "t_trv_int = " << arg.t_trv_int << std::endl;
    std::cout << "t_switch = " << arg.t_switch << std::endl;
    std::cout << "t_ist = " << arg.t_ist << std::endl;
    std::cout << "ray_file = " << arg.ray_file << std::endl;

    std::cout << "loading scene..." << std::endl;
    std::vector<trig_t> trigs = load_trigs(arg.model_file);

    std::cout << "building..." << std::endl;
    bvh_t bvh = build_bvh(trigs);

    std::cout << "clustering..." << std::endl;
    int_bvh_t int_bvh = build_int_bvh(arg.t_trv_int, arg.t_switch, arg.t_ist, trigs, bvh);

    std::cout << "loading rays..." << std::endl;
    std::vector<ray_t> rays;
    std::ifstream ray_fs(arg.ray_file);
    for (float r[7]; ray_fs.read((char*)r, 7 * sizeof(float)); ) {
        rays.emplace_back(
            vector_t(r[0], r[1], r[2]),
            vector_t(r[3], r[4], r[5]),
            0.f,
            r[6]
        );
    }

    // start profiling
    std::cout << "traversing..." << std::endl;
    write(ctl_fd, "enable\n", 8);
    read(ack_fd, ack, 5);
    assert(strncmp(ack, "ack\n", 5) == 0);

    traverser_t full_traverser(bvh);
    primitive_intersector_t primitive_intersector(bvh, trigs.data());
    for (ray_t &ray : rays) {
        auto tmp = full_traverser.traverse(ray, primitive_intersector);
        if (tmp) {
            full_result.t = tmp->intersection.t;
            full_result.u = tmp->intersection.u;
            full_result.v = tmp->intersection.v;
        }
    }

    // finish profiling
    write(ctl_fd, "disable\n", 9);
    read(ack_fd, ack, 5);
    assert(strncmp(ack, "ack\n", 5) == 0);

    std::cout << rays.size() << " rays traversed." << std::endl;
}
