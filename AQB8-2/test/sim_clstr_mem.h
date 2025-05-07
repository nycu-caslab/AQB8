#include <vector>
#include <ac_channel.h>
#include "../include/datatypes.h"
#include "../data/gentypes.h"

void sim_clstr_mem(/* in  */ ac_channel<clstr_mem_req_t>& clstr_mem_req_stream,
                   /* out */ ac_channel<clstr_mem_resp_t>& clstr_mem_resp_stream) {
    static bool initialized = false;
    static std::vector<cluster_t> cluster_mem;
    if (!initialized) {
        initialized = true;
        std::ifstream gen_cluster_fs("../data/gen_cluster.bin", std::ios::binary);
        assert(gen_cluster_fs.good());
        for (gen_cluster_t gen_cluster; gen_cluster_fs.read((char*)(&gen_cluster), sizeof(gen_cluster_t)); ) {
            cluster_t cluster = {
                .x_min = ac_ieee_float32(gen_cluster.x_min).to_ac_std_float(),
                .x_max = ac_ieee_float32(gen_cluster.x_max).to_ac_std_float(),
                .y_min = ac_ieee_float32(gen_cluster.y_min).to_ac_std_float(),
                .y_max = ac_ieee_float32(gen_cluster.y_max).to_ac_std_float(),
                .z_min = ac_ieee_float32(gen_cluster.z_min).to_ac_std_float(),
                .z_max = ac_ieee_float32(gen_cluster.z_max).to_ac_std_float(),
                .inv_sx_inv_sw = ac_ieee_float32(gen_cluster.inv_sx_inv_sw).to_ac_std_float(),
                .nbp_offset = gen_cluster.nbp_offset,
                .trig_offset = gen_cluster.trig_offset
            };
            cluster_mem.push_back(cluster);
        }
    }
    
    while (clstr_mem_req_stream.available(1)) {
        clstr_mem_req_t clstr_mem_req = clstr_mem_req_stream.read();
#ifdef DUMP_TRACE
        trace_fs << "CLSTR " << clstr_mem_req.cluster_idx << "\n";
#endif

        clstr_mem_resp_t clstr_mem_resp = {
            .rid = clstr_mem_req.rid,
            .cluster = cluster_mem[clstr_mem_req.cluster_idx]
        };
        clstr_mem_resp_stream.write(clstr_mem_resp);
    }
}
