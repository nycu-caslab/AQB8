#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <cassert>
#include <numeric>
#include <random>
#include <algorithm>

int main(int argc, char* argv[]) {
    if (argc != 5) {
        std::cerr << "usage: " << argv[0] << " IN_FILE IN_BYTES OUT_FILE OUT_BYTES" << std::endl;
        exit(EXIT_FAILURE);
    }

    char* in_file   = argv[1];
    int   in_bytes  = std::stoi(argv[2]);
    char* out_file  = argv[3];
    int   out_bytes = std::stoi(argv[4]);

    std::ifstream in_fs(in_file, std::ios::binary);
    assert(in_fs.good());
    std::vector<uint8_t> in_;
    for (uint8_t byte; in_fs.read((char*)(&byte), sizeof(uint8_t)); )
        in_.push_back(byte);
    in_fs.close();

    std::ifstream out_fs(out_file, std::ios::binary);
    assert(out_fs.good());
    std::vector<uint8_t> out_;
    for (uint8_t byte; out_fs.read((char*)(&byte), sizeof(uint8_t)); )
        out_.push_back(byte);
    out_fs.close();

    assert(in_.size() % in_bytes == 0);
    assert(out_.size() % out_bytes == 0);
    assert(in_.size() / in_bytes == out_.size() / out_bytes);

    int num_test_vectors = in_.size() / in_bytes;
    std::vector<int> shuffled_index(num_test_vectors);
    std::iota(shuffled_index.begin(), shuffled_index.end(), 0);
    std::shuffle(shuffled_index.begin(), shuffled_index.end(), std::mt19937());

    std::vector<uint8_t> in_shuffled_(in_.size());
    for (int i = 0; i < num_test_vectors; i++)
        for (int j = 0; j < in_bytes; j++)
            in_shuffled_[i*in_bytes+j] = in_[shuffled_index[i]*in_bytes+j];

    std::vector<uint8_t> out_shuffled_(out_.size());
    for (int i = 0; i < num_test_vectors; i++)
        for (int j = 0; j < out_bytes; j++)
            out_shuffled_[i*out_bytes+j] = out_[shuffled_index[i]*out_bytes+j];

    std::ofstream in_shuffled_fs(in_file, std::ios::binary);
    for (uint8_t byte : in_shuffled_)
        in_shuffled_fs.write((char*)(&byte), sizeof(uint8_t));

    std::ofstream out_shuffled_fs(out_file, std::ios::binary);
    for (uint8_t byte : out_shuffled_)
        out_shuffled_fs.write((char*)(&byte), sizeof(uint8_t));
}