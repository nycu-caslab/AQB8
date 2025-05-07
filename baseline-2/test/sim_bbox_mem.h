#include <vector>
#include <ac_channel.h>
#include "../include/datatypes.h"
#include "../data/gentypes.h"

void sim_bbox_mem(/* in  */ ac_channel<bbox_mem_req_t>& bbox_mem_req_stream,
                  /* out */ ac_channel<bbox_mem_resp_t>& bbox_mem_resp_stream) {
    static bool initialized = false;
    static std::vector<nbp_t> nbp_mem;
    if (!initialized) {
        initialized = true;
        std::ifstream gen_nbp_fs("../data/gen_nbp.bin", std::ios::binary);
        assert(gen_nbp_fs.good());
        for (gen_nbp_t gen_nbp; gen_nbp_fs.read((char*)(&gen_nbp), sizeof(gen_nbp_t)); ) {
            nbp_t nbp = {
                .left_node = {
                    .num_trigs = gen_nbp.left_node_num_trigs,
                    .child_idx = gen_nbp.left_node_child_idx
                },
                .right_node = {
                    .num_trigs = gen_nbp.right_node_num_trigs,
                    .child_idx = gen_nbp.right_node_child_idx
                },
                .left_bbox = {
                    .x_min = ac_ieee_float32(gen_nbp.left_bbox_x_min).to_ac_std_float(),
                    .x_max = ac_ieee_float32(gen_nbp.left_bbox_x_max).to_ac_std_float(),
                    .y_min = ac_ieee_float32(gen_nbp.left_bbox_y_min).to_ac_std_float(),
                    .y_max = ac_ieee_float32(gen_nbp.left_bbox_y_max).to_ac_std_float(),
                    .z_min = ac_ieee_float32(gen_nbp.left_bbox_z_min).to_ac_std_float(),
                    .z_max = ac_ieee_float32(gen_nbp.left_bbox_z_max).to_ac_std_float(),
                },
                .right_bbox = {
                    .x_min = ac_ieee_float32(gen_nbp.right_bbox_x_min).to_ac_std_float(),
                    .x_max = ac_ieee_float32(gen_nbp.right_bbox_x_max).to_ac_std_float(),
                    .y_min = ac_ieee_float32(gen_nbp.right_bbox_y_min).to_ac_std_float(),
                    .y_max = ac_ieee_float32(gen_nbp.right_bbox_y_max).to_ac_std_float(),
                    .z_min = ac_ieee_float32(gen_nbp.right_bbox_z_min).to_ac_std_float(),
                    .z_max = ac_ieee_float32(gen_nbp.right_bbox_z_max).to_ac_std_float(),
                }
            };
            nbp_mem.push_back(nbp);
        }
    }
    
    while (bbox_mem_req_stream.available(1)) {
        bbox_mem_req_t bbox_mem_req = bbox_mem_req_stream.read();
#ifdef DUMP_TRACE
        trace_fs << "BBOX " << bbox_mem_req.nbp_idx << "\n";
#endif

        bbox_mem_resp_t bbox_mem_resp = {
            .rid = bbox_mem_req.rid,
            .nbp = nbp_mem[bbox_mem_req.nbp_idx]
        };
        bbox_mem_resp_stream.write(bbox_mem_resp);
    }
}
