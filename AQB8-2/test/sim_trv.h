#include <ac_channel.h>
#include "../include/sim_assert.h"
#include "../include/datatypes.h"

namespace trv_ns {

struct local_mem_t {
    // node_stack
    node_stack_data_t node_stack_[STACK_SIZE];
    stack_size_t node_stack_size;

    // cluster
    bool reintersect;
    bool recompute_qymax;
    cluster_idx_t cluster_idx;
    child_idx_t nbp_offset;
    child_idx_t trig_offset;
    i32_t qb_l_x;
    i32_t qb_l_y;
    i32_t qb_l_z;
    i32_t qy_max;
    fp32_t inv_sx_inv_sw;
    fp32_t y_ref;

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
    field_b_t num_trigs_left;
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

void send_clstr_mem_req(rid_t& rid,
                        local_mem_t (&local_mem_)[NUM_CONCURRENT_RAYS],
                        cluster_idx_t& cluster_idx,
                        ac_channel<clstr_mem_req_t>& clstr_mem_req_stream) {
    local_mem_t& local_mem = local_mem_[rid];
    local_mem.cluster_idx = cluster_idx;

    clstr_mem_req_t clstr_mem_req;
    clstr_mem_req.rid         = rid;
    clstr_mem_req.cluster_idx = cluster_idx;

    clstr_mem_req_stream.write(clstr_mem_req);
}

void send_bbox_mem_req(rid_t& rid,
                       child_idx_t& nbp_idx,
                       ac_channel<bbox_mem_req_t>& bbox_mem_req_stream) {
    bbox_mem_req_t bbox_mem_req;
    bbox_mem_req.rid     = rid;
    bbox_mem_req.nbp_idx = nbp_idx;

    bbox_mem_req_stream.write(bbox_mem_req);
}

void send_ist_mem_req(rid_t& rid,
                      local_mem_t (&local_mem_)[NUM_CONCURRENT_RAYS],
                      field_b_t& num_trigs,
                      child_idx_t& trig_idx,
                      ac_channel<ist_mem_req_t>& ist_mem_req_stream) {
    local_mem_t& local_mem = local_mem_[rid];
    local_mem.num_trigs_left = num_trigs - 1;

    ist_mem_req_t ist_mem_req;
    ist_mem_req.rid       = rid;
    ist_mem_req.num_trigs = num_trigs;
    ist_mem_req.trig_idx  = trig_idx;

    ist_mem_req_stream.write(ist_mem_req);
}

void send_mem_req(rid_t& rid,
                  local_mem_t (&local_mem_)[NUM_CONCURRENT_RAYS],
                  node_t& node,
                  ac_channel<clstr_mem_req_t>& clstr_mem_req_stream,
                  ac_channel<bbox_mem_req_t>& bbox_mem_req_stream,
                  ac_channel<ist_mem_req_t>& ist_mem_req_stream) {
    local_mem_t& local_mem = local_mem_[rid];
    if (node.field_a) {
        if (node.field_b == 0) {
            // INTERNAL
            child_idx_t nbp_idx = local_mem.nbp_offset + node.field_c;
            send_bbox_mem_req(rid, nbp_idx, bbox_mem_req_stream);
        } else {
            // LEAF
            child_idx_t trig_idx = local_mem.trig_offset + node.field_c;
            send_ist_mem_req(rid, local_mem_, node.field_b, trig_idx, ist_mem_req_stream);
        }
    } else {
        // SWITCH
        cluster_idx_t cluster_idx;
        cluster_idx.set_slc(FIELD_C_WIDTH, node.field_b);
        cluster_idx.set_slc(0, node.field_c);
        send_clstr_mem_req(rid, local_mem_, cluster_idx, clstr_mem_req_stream);
    }
}

void stack_op(rid_t& rid,
              local_mem_t (&local_mem_)[NUM_CONCURRENT_RAYS],
              ac_channel<clstr_mem_req_t>& clstr_mem_req_stream,
              ac_channel<updt_req_t>& updt_req_stream,
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
        // peek node_stack
        stack_size_t node_stack_size_minus_one = local_mem.node_stack_size - 1;
        node_stack_data_t node_stack_top = local_mem.node_stack_[node_stack_size_minus_one];

        if (node_stack_top.cluster_idx != local_mem.cluster_idx && node_stack_top.node.field_a && node_stack_top.node.field_b == 0) {
            local_mem.reintersect = true;
            send_clstr_mem_req(rid, local_mem_, node_stack_top.cluster_idx, clstr_mem_req_stream);
        } else if (local_mem.recompute_qymax && node_stack_top.node.field_a && node_stack_top.node.field_b == 0) {
            updt_req_t updt_req;
            updt_req.rid = rid;
            updt_req.inv_sx_inv_sw = local_mem.inv_sx_inv_sw;
            updt_req.y_ref = local_mem.y_ref;
            updt_req.tmax = local_mem.all_ray.tmax;

            updt_req_stream.write(updt_req);
        } else {
            // pop node_stack
            local_mem.node_stack_size = node_stack_size_minus_one;
            send_mem_req(rid, local_mem_, node_stack_top.node, clstr_mem_req_stream, bbox_mem_req_stream, ist_mem_req_stream);
        }
    }
}

}  // namespace trv_ns

#pragma hls_design
#pragma hls_resource trig_rsc variables="trig_" map_to_module="ccs_sample_mem.ccs_ram_sync_1R1W"
void sim_trv(/* in  */ ac_channel<trv_req_t>& trv_req_stream,
             /* in  */ ac_channel<clstr_mem_resp_t>& clstr_mem_resp_stream,
             /* in  */ ac_channel<clstr_resp_t>& clstr_resp_stream,
             /* in  */ ac_channel<updt_resp_t>& updt_resp_stream,
             /* in  */ ac_channel<bbox_mem_resp_t>& bbox_mem_resp_stream,
             /* in  */ ac_channel<bbox_resp_t>& bbox_resp_stream,
             /* in  */ ac_channel<ist_mem_resp_t>& ist_mem_resp_stream,
             /* in  */ ac_channel<ist_resp_t>& ist_resp_stream,
             /* in  */ trig_t trig_[TRIG_SRAM_DEPTH],
             /* out */ ac_channel<clstr_mem_req_t>& clstr_mem_req_stream,
             /* out */ ac_channel<clstr_req_t>& clstr_req_stream,
             /* out */ ac_channel<updt_req_t>& updt_req_stream,
             /* out */ ac_channel<bbox_mem_req_t>& bbox_mem_req_stream,
             /* out */ ac_channel<bbox_req_t>& bbox_req_stream,
             /* out */ ac_channel<ist_mem_req_t>& ist_mem_req_stream,
             /* out */ ac_channel<ist_req_t>& ist_req_stream,
             /* out */ ac_channel<trv_resp_t>& trv_resp_stream) {
    using namespace trv_ns;

    static local_mem_t local_mem_[NUM_CONCURRENT_RAYS];

    trv_req_t trv_req;
    clstr_mem_resp_t clstr_mem_resp;
    clstr_resp_t clstr_resp;
    updt_resp_t updt_resp;
    bbox_mem_resp_t bbox_mem_resp;
    bbox_resp_t bbox_resp;
    ist_mem_resp_t ist_mem_resp;
    ist_resp_t ist_resp;

    if (trv_req_stream.nb_read(trv_req)) {
        local_mem_t& local_mem = local_mem_[trv_req.rid];
        local_mem.node_stack_size = 0;
        local_mem.reintersect = false;
        local_mem.recompute_qymax = false;
        local_mem.all_ray = trv_req.all_ray;
        local_mem.intersected = false;

        cluster_idx_t cluster_idx = 0;
        send_clstr_mem_req(trv_req.rid, local_mem_, cluster_idx, clstr_mem_req_stream);
    } else if (clstr_mem_resp_stream.nb_read(clstr_mem_resp)) {
        local_mem_t& local_mem = local_mem_[clstr_mem_resp.rid];
        local_mem.nbp_offset = clstr_mem_resp.cluster.nbp_offset;
        local_mem.trig_offset = clstr_mem_resp.cluster.trig_offset;

        clstr_req_t clstr_req;
        clstr_req.rid                   = clstr_mem_resp.rid;
        clstr_req.preprocessed_ray.w_x  = local_mem.all_ray.w_x;
        clstr_req.preprocessed_ray.w_y  = local_mem.all_ray.w_y;
        clstr_req.preprocessed_ray.w_z  = local_mem.all_ray.w_z;
        clstr_req.preprocessed_ray.b_x  = local_mem.all_ray.b_x;
        clstr_req.preprocessed_ray.b_y  = local_mem.all_ray.b_y;
        clstr_req.preprocessed_ray.b_z  = local_mem.all_ray.b_z;
        clstr_req.preprocessed_ray.tmin = local_mem.all_ray.tmin;
        clstr_req.preprocessed_ray.tmax = local_mem.all_ray.tmax;
        clstr_req.inv_sx_inv_sw         = clstr_mem_resp.cluster.inv_sx_inv_sw;
        clstr_req.x_min                 = clstr_mem_resp.cluster.x_min;
        clstr_req.x_max                 = clstr_mem_resp.cluster.x_max;
        clstr_req.y_min                 = clstr_mem_resp.cluster.y_min;
        clstr_req.y_max                 = clstr_mem_resp.cluster.y_max;
        clstr_req.z_min                 = clstr_mem_resp.cluster.z_min;
        clstr_req.z_max                 = clstr_mem_resp.cluster.z_max;

        clstr_req_stream.write(clstr_req);
    } else if (clstr_resp_stream.nb_read(clstr_resp)) {
        local_mem_t& local_mem = local_mem_[clstr_resp.rid];

        bool reintersect = local_mem.reintersect;
        local_mem.reintersect = false;
        local_mem.recompute_qymax = false;

        if (clstr_resp.intersected || reintersect) {
            local_mem.qb_l_x        = clstr_resp.qb_l_x;
            local_mem.qb_l_y        = clstr_resp.qb_l_y;
            local_mem.qb_l_z        = clstr_resp.qb_l_z;
            local_mem.qy_max        = clstr_resp.qy_max;
            local_mem.inv_sx_inv_sw = clstr_resp.inv_sx_inv_sw;
            local_mem.y_ref         = clstr_resp.y_ref;
        }

        if (clstr_resp.intersected && (!reintersect))
            send_bbox_mem_req(clstr_resp.rid, local_mem.nbp_offset, bbox_mem_req_stream);
        else
            stack_op(clstr_resp.rid, local_mem_, clstr_mem_req_stream, updt_req_stream, bbox_mem_req_stream, ist_mem_req_stream, trv_resp_stream);
    } else if (updt_resp_stream.nb_read(updt_resp)) {
        local_mem_t& local_mem = local_mem_[updt_resp.rid];
        local_mem.recompute_qymax = false;
        local_mem.qy_max = updt_resp.qy_max;
        stack_op(updt_resp.rid, local_mem_, clstr_mem_req_stream, updt_req_stream, bbox_mem_req_stream, ist_mem_req_stream, trv_resp_stream);
    } else if (bbox_mem_resp_stream.nb_read(bbox_mem_resp)) {
        local_mem_t& local_mem = local_mem_[bbox_mem_resp.rid];
        local_mem.left_node = bbox_mem_resp.nbp.left_node;
        local_mem.right_node = bbox_mem_resp.nbp.right_node;
        
        bbox_req_t bbox_req;
        bbox_req.rid              = bbox_mem_resp.rid;
        bbox_req.quant_ray.iw_x   = local_mem.all_ray.w_x.data_ac_int()[31];
        bbox_req.quant_ray.iw_y   = local_mem.all_ray.w_y.data_ac_int()[31];
        bbox_req.quant_ray.iw_z   = local_mem.all_ray.w_z.data_ac_int()[31];
        bbox_req.quant_ray.qw_l_x = local_mem.all_ray.qw_l_x;
        bbox_req.quant_ray.qw_l_y = local_mem.all_ray.qw_l_y;
        bbox_req.quant_ray.qw_l_z = local_mem.all_ray.qw_l_z;
        bbox_req.quant_ray.qw_h_x = local_mem.all_ray.qw_h_x;
        bbox_req.quant_ray.qw_h_y = local_mem.all_ray.qw_h_y;
        bbox_req.quant_ray.qw_h_z = local_mem.all_ray.qw_h_z;
        bbox_req.quant_ray.rw_l_x = local_mem.all_ray.rw_l_x;
        bbox_req.quant_ray.rw_l_y = local_mem.all_ray.rw_l_y;
        bbox_req.quant_ray.rw_l_z = local_mem.all_ray.rw_l_z;
        bbox_req.quant_ray.rw_h_x = local_mem.all_ray.rw_h_x;
        bbox_req.quant_ray.rw_h_y = local_mem.all_ray.rw_h_y;
        bbox_req.quant_ray.rw_h_z = local_mem.all_ray.rw_h_z;
        bbox_req.quant_ray.qb_l_x = local_mem.qb_l_x;
        bbox_req.quant_ray.qb_l_y = local_mem.qb_l_y;
        bbox_req.quant_ray.qb_l_z = local_mem.qb_l_z;
        bbox_req.quant_ray.qy_max = local_mem.qy_max;
        bbox_req.left_bbox        = bbox_mem_resp.nbp.left_bbox;
        bbox_req.right_bbox       = bbox_mem_resp.nbp.right_bbox;

        bbox_req_stream.write(bbox_req);
    } else if (bbox_resp_stream.nb_read(bbox_resp)) {
        local_mem_t& local_mem = local_mem_[bbox_resp.rid];
        if (bbox_resp.left_hit) {
            if (bbox_resp.right_hit) {
                bool left_is_leaf = local_mem.left_node.field_a && (local_mem.left_node.field_b != 0);
                bool right_is_leaf = local_mem.right_node.field_a && (local_mem.right_node.field_b != 0);
                bool left_first = left_is_leaf || (!right_is_leaf && bbox_resp.left_first);
                node_t& first_node = left_first ? local_mem.left_node : local_mem.right_node;
                node_t& second_node = left_first ? local_mem.right_node : local_mem.left_node;
                
                // push to node_stack
                sim_assert(local_mem.node_stack_size != STACK_SIZE);
                local_mem.node_stack_[local_mem.node_stack_size].node        = second_node;
                local_mem.node_stack_[local_mem.node_stack_size].cluster_idx = local_mem.cluster_idx;
                local_mem.node_stack_size++;
                node_stack_max_size = std::max(node_stack_max_size, local_mem.node_stack_size.to_int());

                send_mem_req(bbox_resp.rid, local_mem_, first_node, clstr_mem_req_stream, bbox_mem_req_stream, ist_mem_req_stream);
            } else {
                send_mem_req(bbox_resp.rid, local_mem_, local_mem.left_node, clstr_mem_req_stream, bbox_mem_req_stream, ist_mem_req_stream);
            }
        } else if (bbox_resp.right_hit) {
            send_mem_req(bbox_resp.rid, local_mem_, local_mem.right_node, clstr_mem_req_stream, bbox_mem_req_stream, ist_mem_req_stream);
        } else {
            stack_op(bbox_resp.rid, local_mem_, clstr_mem_req_stream, updt_req_stream, bbox_mem_req_stream, ist_mem_req_stream, trv_resp_stream);
        }
    } else if (ist_mem_resp_stream.nb_read(ist_mem_resp)) {
        send_ist_req(ist_mem_resp.rid, local_mem_, trig_, ist_req_stream);
    } else if (ist_resp_stream.nb_read(ist_resp)) {
        local_mem_t& local_mem = local_mem_[ist_resp.rid];
        field_b_t num_trigs_left = local_mem.num_trigs_left;
        local_mem.num_trigs_left = num_trigs_left - 1;
        if (ist_resp.intersected) {
            local_mem.recompute_qymax = true;
            local_mem.all_ray.tmax    = ist_resp.t;
            local_mem.intersected     = true;
            local_mem.u               = ist_resp.u;
            local_mem.v               = ist_resp.v;
        }

        if (num_trigs_left == 0)
            stack_op(ist_resp.rid, local_mem_, clstr_mem_req_stream, updt_req_stream, bbox_mem_req_stream, ist_mem_req_stream, trv_resp_stream);
        else
            send_ist_req(ist_resp.rid, local_mem_, trig_, ist_req_stream);
    }
}
