#include <fstream>

#ifdef DUMP_TRACE
std::ofstream trace_fs("trace.txt");
#endif

int node_stack_max_size = 0;

#include "sim_rtcore.h"

int main() {
    std::ifstream ray_fs("../data/ray.bin", std::ios::binary);
    assert(ray_fs.good());
    std::vector<ray_t> ray_;
    for (ray_t ray; ray_fs.read((char*)(&ray), sizeof(ray_t)); )
        ray_.push_back(ray);

    std::ifstream result_fs("../data/result.bin", std::ios::binary);
    assert(result_fs.good());
    std::vector<result_t> result_;
    for (result_t result; result_fs.read((char*)(&result), sizeof(result_t)); )
        result_.push_back(result);

    assert(ray_.size() == result_.size());

    ac_channel<ray_t> ray_stream;
    ac_channel<result_t> result_stream;

    for (auto &ray : ray_)
        ray_stream.write(ray);

    uintmax_t num_correct = 0;
    for (size_t i = 0; i < ray_.size(); i++) {
        while (!result_stream.available(1))
            sim_rtcore(ray_stream, result_stream);
        result_t result = result_stream.read();
        bool correct = false;
        if (result_[i].intersected) {
            if (result.intersected && 
                result.t == result_[i].t &&
                result.u == result_[i].u &&
                result.v == result_[i].v)
                correct = true;
        } else {
            if (!result.intersected)
                correct = true;
        }
        if (correct) {
            num_correct++;
        } else {
            std::cout << "differ in " << i << std::endl; 
            std::cout << "  " << result.intersected << " (golden = " << result_[i].intersected << ")" << std::endl;
            std::cout << "  " << result.t           << " (golden = " << result_[i].t           << ")" << std::endl;
            std::cout << "  " << result.u           << " (golden = " << result_[i].u           << ")" << std::endl;
            std::cout << "  " << result.v           << " (golden = " << result_[i].v           << ")" << std::endl;
        }
    }

    std::cout << "num_correct = " << num_correct << std::endl;
    std::cout << "ray_.size() = " << ray_.size() << std::endl;

    std::cout << "node_stack_max_size = " << node_stack_max_size << std::endl;

    return 0;
}