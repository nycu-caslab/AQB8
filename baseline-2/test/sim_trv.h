#include <ac_channel.h>
#include "../include/sim_assert.h"
#include "../include/datatypes.h"

namespace trv_ns {

struct local_mem_t {
    // node_stack
    node_t node_stack_[STACK_SIZE];
    stack_size_t node_stack_size;

    // ray
    all_ray_t all_ray;

    // result
    bool intersected;
    fp32_t u;
    fp32_t v;

    // bbox_mem
    node_t left_node;
    node_t right_node;

    // ist_mem
    num_trigs_t num_trigs_left;
};

void send_ist_req(rid_t& rid,
                  local_mem_t (&local_mem_)[NUM_CONCURRENT_RAYS],
                  trig_t trig_[TRIG_SRAM_DEPTH],
                  ac_channel<ist_req_t>& ist_req_stream) {
    local_mem_t& local_mem = local_mem_[rid];
    
    ist_req_t ist_req;
    ist_req.rid          = rid;
    ist_req.ray.origin_x = local_mem.all_ray.origin_x;
    ist_req.ray.origin_y = local_mem.all_ray.origin_y;
    ist_req.ray.origin_z = local_mem.all_ray.origin_z;
    ist_req.ray.dir_x    = local_mem.all_ray.dir_x;
    ist_req.ray.dir_y    = local_mem.all_ray.dir_y;
    ist_req.ray.dir_z    = local_mem.all_ray.dir_z;
    ist_req.ray.tmin     = local_mem.all_ray.tmin;
    ist_req.ray.tmax     = local_mem.all_ray.tmax;
    ist_req.trig         = trig_[local_mem.num_trigs_left * NUM_CONCURRENT_RAYS + rid];

    ist_req_stream.write(ist_req);
}

void send_mem_req(rid_t& rid,
                  local_mem_t (&local_mem_)[NUM_CONCURRENT_RAYS],
                  node_t& node,
                  ac_channel<bbox_mem_req_t>& bbox_mem_req_stream,
                  ac_channel<ist_mem_req_t>& ist_mem_req_stream) {
    if (node.num_trigs == 0) {
        bbox_mem_req_t bbox_mem_req;
        bbox_mem_req.rid     = rid;
        bbox_mem_req.nbp_idx = node.child_idx;

        bbox_mem_req_stream.write(bbox_mem_req);
    } else {
        local_mem_t& local_mem = local_mem_[rid];
        local_mem.num_trigs_left = node.num_trigs - 1;

        ist_mem_req_t ist_mem_req;
        ist_mem_req.rid       = rid;
        ist_mem_req.num_trigs = node.num_trigs;
        ist_mem_req.trig_idx  = node.child_idx;

        ist_mem_req_stream.write(ist_mem_req);
    }
}

void stack_op(rid_t& rid,
              local_mem_t (&local_mem_)[NUM_CONCURRENT_RAYS],
              ac_channel<bbox_mem_req_t>& bbox_mem_req_stream,
              ac_channel<ist_mem_req_t>& ist_mem_req_stream,
              ac_channel<trv_resp_t>& trv_resp_stream) {
    local_mem_t& local_mem = local_mem_[rid];
    if (local_mem.node_stack_size == 0) {
        trv_resp_t trv_resp;
        trv_resp.rid                = rid;
        trv_resp.result.intersected = local_mem.intersected;
        trv_resp.result.t           = local_mem.all_ray.tmax;
        trv_resp.result.u           = local_mem.u;
        trv_resp.result.v           = local_mem.v;

        trv_resp_stream.write(trv_resp);
    } else {
        // pop node_stack
        stack_size_t node_stack_size_minus_one = local_mem.node_stack_size - 1;
        node_t node_stack_top = local_mem.node_stack_[node_stack_size_minus_one];
        local_mem.node_stack_size = node_stack_size_minus_one;
        send_mem_req(rid, local_mem_, node_stack_top, bbox_mem_req_stream, ist_mem_req_stream);
    }
}

}  // namespace trv_ns

#pragma hls_design
#pragma hls_resource trig_rsc variables="trig_" map_to_module="ccs_sample_mem.ccs_ram_sync_1R1W"
void sim_trv(/* in  */ ac_channel<trv_req_t>& trv_req_stream,
             /* in  */ ac_channel<bbox_mem_resp_t>& bbox_mem_resp_stream,
             /* in  */ ac_channel<bbox_resp_t>& bbox_resp_stream,
             /* in  */ ac_channel<ist_mem_resp_t>& ist_mem_resp_stream,
             /* in  */ trig_t trig_[TRIG_SRAM_DEPTH],
             /* in  */ ac_channel<ist_resp_t>& ist_resp_stream,
             /* out */ ac_channel<bbox_mem_req_t>& bbox_mem_req_stream,
             /* out */ ac_channel<bbox_req_t>& bbox_req_stream,
             /* out */ ac_channel<ist_mem_req_t>& ist_mem_req_stream,
             /* out */ ac_channel<ist_req_t>& ist_req_stream,
             /* out */ ac_channel<trv_resp_t>& trv_resp_stream) {
    using namespace trv_ns;

    static local_mem_t local_mem_[NUM_CONCURRENT_RAYS];

    trv_req_t trv_req;
    bbox_mem_resp_t bbox_mem_resp;
    bbox_resp_t bbox_resp;
    ist_mem_resp_t ist_mem_resp;
    ist_resp_t ist_resp;

    if (trv_req_stream.nb_read(trv_req)) {
        local_mem_t& local_mem = local_mem_[trv_req.rid];
        local_mem.node_stack_size = 0;
        local_mem.all_ray = trv_req.all_ray;
        local_mem.intersected = false;

        bbox_mem_req_t bbox_mem_req;
        bbox_mem_req.rid     = trv_req.rid;
        bbox_mem_req.nbp_idx = 0;

        bbox_mem_req_stream.write(bbox_mem_req);
    } else if (bbox_mem_resp_stream.nb_read(bbox_mem_resp)) {
        local_mem_t& local_mem = local_mem_[bbox_mem_resp.rid];
        local_mem.left_node = bbox_mem_resp.nbp.left_node;
        local_mem.right_node = bbox_mem_resp.nbp.right_node;

        bbox_req_t bbox_req;
        bbox_req.rid                   = bbox_mem_resp.rid;
        bbox_req.preprocessed_ray.w_x  = local_mem.all_ray.w_x;
        bbox_req.preprocessed_ray.w_y  = local_mem.all_ray.w_y;
        bbox_req.preprocessed_ray.w_z  = local_mem.all_ray.w_z;
        bbox_req.preprocessed_ray.b_x  = local_mem.all_ray.b_x;
        bbox_req.preprocessed_ray.b_y  = local_mem.all_ray.b_y;
        bbox_req.preprocessed_ray.b_z  = local_mem.all_ray.b_z;
        bbox_req.preprocessed_ray.tmin = local_mem.all_ray.tmin;
        bbox_req.preprocessed_ray.tmax = local_mem.all_ray.tmax;
        bbox_req.left_bbox             = bbox_mem_resp.nbp.left_bbox;
        bbox_req.right_bbox            = bbox_mem_resp.nbp.right_bbox;

        bbox_req_stream.write(bbox_req);
    } else if (bbox_resp_stream.nb_read(bbox_resp)) {
        local_mem_t& local_mem = local_mem_[bbox_resp.rid];
        if (bbox_resp.left_hit) {
            if (bbox_resp.right_hit) {
                bool left_is_leaf = (local_mem.left_node.num_trigs != 0);
                bool right_is_leaf = (local_mem.right_node.num_trigs != 0);
                bool left_first = left_is_leaf || (!right_is_leaf && bbox_resp.left_first);
                node_t& first_node = left_first ? local_mem.left_node : local_mem.right_node;
                node_t& second_node = left_first ? local_mem.right_node : local_mem.left_node;
                
                // push to node_stack
                sim_assert(local_mem.node_stack_size != STACK_SIZE);
                local_mem.node_stack_[local_mem.node_stack_size++] = second_node;
                node_stack_max_size = std::max(node_stack_max_size, local_mem.node_stack_size.to_int());

                send_mem_req(bbox_resp.rid, local_mem_, first_node, bbox_mem_req_stream, ist_mem_req_stream);
            } else {
                send_mem_req(bbox_resp.rid, local_mem_, local_mem.left_node, bbox_mem_req_stream, ist_mem_req_stream);
            }
        } else if (bbox_resp.right_hit) {
            send_mem_req(bbox_resp.rid, local_mem_, local_mem.right_node, bbox_mem_req_stream, ist_mem_req_stream);
        } else {
            stack_op(bbox_resp.rid, local_mem_, bbox_mem_req_stream, ist_mem_req_stream, trv_resp_stream);
        }
    } else if (ist_mem_resp_stream.nb_read(ist_mem_resp)) {
        send_ist_req(ist_mem_resp.rid, local_mem_, trig_, ist_req_stream);
    } else if (ist_resp_stream.nb_read(ist_resp)) {
        local_mem_t& local_mem = local_mem_[ist_resp.rid];
        num_trigs_t num_trigs_left = local_mem.num_trigs_left;
        local_mem.num_trigs_left = num_trigs_left - 1;
        if (ist_resp.intersected) {
            local_mem.all_ray.tmax = ist_resp.t;
            local_mem.intersected  = true;
            local_mem.u            = ist_resp.u;
            local_mem.v            = ist_resp.v;
        }

        if (num_trigs_left == 0)
            stack_op(ist_resp.rid, local_mem_, bbox_mem_req_stream, ist_mem_req_stream, trv_resp_stream);
        else
            send_ist_req(ist_resp.rid, local_mem_, trig_, ist_req_stream);
    }
}
