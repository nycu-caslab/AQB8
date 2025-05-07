#include <ac_channel.h>
#include "../include/datatypes.h"
#include "../data/gentypes.h"

void sim_reorder(/* in  */ ac_channel<ray_t>& ray_stream,
                 /* in  */ ac_channel<trv_resp_t>& trv_resp_stream,
                 /* out */ ac_channel<init_req_t>& init_req_stream,
                 /* out */ ac_channel<result_t>& result_stream) {
    static bool initialized = false;
    static ref_bbox_t ref_bbox;
    if (!initialized) {
        initialized = true;

        std::ifstream gen_ref_bbox_fs("../data/gen_ref_bbox.bin", std::ios::binary);
        assert(gen_ref_bbox_fs.good());

        gen_ref_bbox_t gen_ref_bbox;
        gen_ref_bbox_fs.read((char*)(&gen_ref_bbox), sizeof(gen_ref_bbox_t));

        ref_bbox = {
            .x_min = ac_ieee_float32(gen_ref_bbox.x_min).to_ac_std_float(),
            .y_min = ac_ieee_float32(gen_ref_bbox.y_min).to_ac_std_float(),
            .z_min = ac_ieee_float32(gen_ref_bbox.z_min).to_ac_std_float()
        };
    }
    
#ifdef FIX_CID_TO_ZERO
    static bool processing = false;

    ray_t ray;
    trv_resp_t trv_resp;

    if (!processing && ray_stream.nb_read(ray)) {
        processing = true;
        init_req_t init_req = {
            .rid = 0,
            .ray = ray,
            .ref_bbox = ref_bbox
        };
        init_req_stream.write(init_req);
    } else if (trv_resp_stream.nb_read(trv_resp)) {
        processing = false;
        result_stream.write(trv_resp.result);
    }
#else  // FIX_CID_TO_ZERO
    enum rid_state_t {
        IDLE, PROCESSING, WAITING
    };

    static bool initialized = false;
    static rid_state_t rid_state[NUM_CONCURRENT_RAYS];
    static int ray_ptr = 0;
    static int result_ptr = 0;
    static int mapper[NUM_CONCURRENT_RAYS];
    static result_t result[NUM_CONCURRENT_RAYS];

    if (!initialized) {
        initialized = true;
        for (int i = 0; i < NUM_CONCURRENT_RAYS; i++)
            rid_state[i] = IDLE;
    }

    bool ready_to_receive = false;
    int rid;
    for (int i = 0; i < NUM_CONCURRENT_RAYS; i++) {
        if (rid_state[i] == IDLE) {
            ready_to_receive = true;
            rid = i;
            break;
        }
    }

    ray_t ray;
    trv_resp_t trv_resp;

    if (ready_to_receive && ray_stream.nb_read(ray)) {
        rid_state[rid] = PROCESSING;
        mapper[ray_ptr] = rid;
        ray_ptr = (ray_ptr + 1) % NUM_CONCURRENT_RAYS;
        init_req_t init_req = {
            .rid = rid,
            .ray = ray,
            .ref_bbox = ref_bbox
        };
        init_req_stream.write(init_req);
    } else if (trv_resp_stream.nb_read(trv_resp)) {
        rid_state[trv_resp.rid] = WAITING;
        result[trv_resp.rid] = trv_resp.result;
        for (; rid_state[mapper[result_ptr]] == WAITING; result_ptr = (result_ptr + 1) % NUM_CONCURRENT_RAYS) {
            rid_state[mapper[result_ptr]] = IDLE;
            result_stream.write(result[mapper[result_ptr]]);
        }
    }
#endif  // FIX_CID_TO_ZERO
}
