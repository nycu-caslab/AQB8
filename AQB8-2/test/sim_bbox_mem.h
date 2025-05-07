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
                    .field_a = (bool)gen_nbp.left_node_field_a,
                    .field_b = gen_nbp.left_node_field_b,
                    .field_c = gen_nbp.left_node_field_c
                },
                .right_node = {
                    .field_a = (bool)gen_nbp.right_node_field_a,
                    .field_b = gen_nbp.right_node_field_b,
                    .field_c = gen_nbp.right_node_field_c
                },
                .left_bbox = {
                    .x_min = gen_nbp.left_bbox_x_min,
                    .x_max = gen_nbp.left_bbox_x_max,
                    .y_min = gen_nbp.left_bbox_y_min,
                    .y_max = gen_nbp.left_bbox_y_max,
                    .z_min = gen_nbp.left_bbox_z_min,
                    .z_max = gen_nbp.left_bbox_z_max,
                },
                .right_bbox = {
                    .x_min = gen_nbp.right_bbox_x_min,
                    .x_max = gen_nbp.right_bbox_x_max,
                    .y_min = gen_nbp.right_bbox_y_min,
                    .y_max = gen_nbp.right_bbox_y_max,
                    .z_min = gen_nbp.right_bbox_z_min,
                    .z_max = gen_nbp.right_bbox_z_max,
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
