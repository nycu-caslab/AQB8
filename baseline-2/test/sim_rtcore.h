#include "sim_reorder.h"
#include "sim_trv.h"
#include "sim_bbox_mem.h"
#include "sim_ist_mem.h"

extern void rtcore(/* in  */ ac_channel<init_req_t>& init_req_stream,
                   /* in  */ ac_channel<bbox_req_t>& bbox_req_stream,
                   /* in  */ ac_channel<ist_req_t>& ist_req_stream,
                   /* out */ ac_channel<trv_req_t>& trv_req_stream,
                   /* out */ ac_channel<bbox_resp_t>& bbox_resp_stream,
                   /* out */ ac_channel<ist_resp_t>& ist_resp_stream);

void sim_rtcore(/* in  */ ac_channel<ray_t>& ray_stream,
                /* out */ ac_channel<result_t>& result_stream) {
    static ac_channel<init_req_t> init_req_stream;
    static ac_channel<trv_resp_t> trv_resp_stream;
    static ac_channel<trv_req_t> trv_req_stream;
    static ac_channel<bbox_mem_req_t> bbox_mem_req_stream;
    static ac_channel<bbox_mem_resp_t> bbox_mem_resp_stream;
    static ac_channel<bbox_req_t> bbox_req_stream;
    static ac_channel<bbox_resp_t> bbox_resp_stream;
    static ac_channel<ist_mem_req_t> ist_mem_req_stream;
    static ac_channel<ist_mem_resp_t> ist_mem_resp_stream;
    static ac_channel<ist_req_t> ist_req_stream;
    static ac_channel<ist_resp_t> ist_resp_stream;
    static trig_t trig_[TRIG_SRAM_DEPTH];

    sim_reorder(ray_stream, trv_resp_stream, init_req_stream, result_stream);
    sim_trv(trv_req_stream, bbox_mem_resp_stream, bbox_resp_stream, ist_mem_resp_stream, trig_, ist_resp_stream, bbox_mem_req_stream, bbox_req_stream, ist_mem_req_stream, ist_req_stream, trv_resp_stream);
    sim_bbox_mem(bbox_mem_req_stream, bbox_mem_resp_stream);
    sim_ist_mem(ist_mem_req_stream, ist_mem_resp_stream, trig_);

    rtcore(init_req_stream, bbox_req_stream, ist_req_stream, trv_req_stream, bbox_resp_stream, ist_resp_stream);
}
