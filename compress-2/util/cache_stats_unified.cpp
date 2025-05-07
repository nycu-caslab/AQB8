#include <iostream>
#include <fstream>
#include <sstream>
#include <cassert>
#include <vector>
#include <unordered_map>

const int NBP_BASEADDR     = 0x10000000;
const int TRIG_BASEADDR    = 0x40000000;

const int NBP_SIZE     = 23;
const int TRIG_SIZE    = 36;

struct line_t {
    bool valid;
    int idx;
};

struct cache_t {
    int sets;
    int ways;
    int clsize;
    int wordsize;
    
    int hit;
    int miss;
    std::unordered_map<int, int> miss_types;

    std::ofstream miss_trace_fs;
    cache_t* next_level_cache;

    std::vector<line_t> lines;
    
    cache_t(int sets, int ways, int clsize, int wordsize,
            const char* miss_trace, cache_t* next_level_cache) : sets(sets), ways(ways), clsize(clsize), wordsize(wordsize),
                                                                 hit(0), miss(0), miss_trace_fs(miss_trace), next_level_cache(next_level_cache) {
        assert(clsize % wordsize == 0);
        lines.resize(sets * ways);
        for (int i = 0; i < sets * ways; i++)
            lines[i].valid = false;
    }

    void load_word(int word, int type) {
        int idx = word / (clsize / wordsize);
        int set = idx % sets;

        // hit
        for (int way = 0; way < ways; way++) {
            if (lines[set * ways + way].valid && lines[set * ways + way].idx == idx) {
                line_t hit_line = lines[set * ways + way];
                for (int i = way; i >= 1; i--)
                    lines[set * ways + i] = lines[set * ways + i - 1];
                lines[set * ways + 0] = hit_line;
                hit++;
                return;
            }
        }

        // miss
        for (int way = ways - 1; way > 0; way--)
            lines[set * ways + way] = lines[set * ways + way - 1];
        lines[set * ways + 0].valid = true;
        lines[set * ways + 0].idx = idx;
        int addr = idx * clsize;
        if (next_level_cache != nullptr)
            next_level_cache->load(addr, clsize, type);
        char buffer[9];
        sprintf(buffer, "%08x", addr);
        miss_trace_fs << "0x" << buffer << " R" << std::endl;
        miss++;
        miss_types[type]++;
    }

    void load(int addr, int length, int type) {
        int start = addr / wordsize;
        int end = (addr + length - 1) / wordsize;
        for (int word = start; word <= end; word++)
            load_word(word, type);
    }

    void print_stats() {
        std::cout << "hit = " << hit << std::endl;
        std::cout << "miss = " << miss << std::endl;
        for (auto &miss_type : miss_types)
            std::cout << miss_type.first << ", " << miss_type.second << std::endl;
    }
};

int main(int argc, char* argv[]) {
    if (argc != 8) {
        std::cout << "usage: " << argv[0] << " TRACE_FILE L1_SETS L1_WAYS L2_SETS L2_WAYS CLSIZE WORDSIZE" << std::endl;
        exit(EXIT_FAILURE);
    }

    char* trace_file = argv[1];
    int   l1_sets    = std::stoi(argv[2]);
    int   l1_ways    = std::stoi(argv[3]);
    int   l2_sets    = std::stoi(argv[4]);
    int   l2_ways    = std::stoi(argv[5]);
    int   clsize     = std::stoi(argv[6]);
    int   wordsize   = std::stoi(argv[7]);

    std::cout << "TRACE_FILE = " << trace_file << std::endl;
    std::cout << "L1_SETS = "    << l1_sets    << std::endl;
    std::cout << "L1_WAYS = "    << l1_ways    << std::endl;
    std::cout << "L2_SETS = "    << l2_sets    << std::endl;
    std::cout << "L2_WAYS = "    << l2_ways    << std::endl;
    std::cout << "CLSIZE = "     << clsize     << std::endl;
    std::cout << "WORDSIZE = "   << wordsize   << std::endl;

    cache_t l2_cache(l2_sets, l2_ways, clsize, clsize, "dram_trace.txt", nullptr);
    cache_t l1_cache(l1_sets, l1_ways, clsize, wordsize, "/dev/null", &l2_cache);

    std::ifstream trace_fs(trace_file);
    for (std::string line; std::getline(trace_fs, line); ) {
        std::istringstream ss(line);
        std::string type;
        ss >> type;
        if (type == "BBOX") {
            int nbp_idx;
            assert(ss >> nbp_idx);
            int nbp_addr = NBP_BASEADDR + nbp_idx * NBP_SIZE;
            assert(NBP_BASEADDR <= nbp_addr && nbp_addr < TRIG_BASEADDR);
            l1_cache.load(nbp_addr, NBP_SIZE, 0);
        } else if (type == "IST") {
            int num_trigs;
            int trig_idx;
            assert(ss >> num_trigs);
            assert(ss >> trig_idx);
            int trig_addr = TRIG_BASEADDR + trig_idx * TRIG_SIZE;
            assert(TRIG_BASEADDR <= trig_addr);
            l1_cache.load(trig_addr, num_trigs * TRIG_SIZE, 1);
        } else {
            assert(false);
        }
    }

    l1_cache.print_stats();
    l2_cache.print_stats();
}
