#include <ccs_dw_fp_lib.h>
#include <ac_channel.h>
#include "../include/fp_cmp.h"
#include "../include/datatypes.h"

#ifdef DUMP_TRACE
#include <fstream>
std::ofstream bbox_req_fs("bbox_req.bin", std::ios::binary);
std::ofstream bbox_resp_fs("bbox_resp.bin", std::ios::binary);
#endif

namespace bbox_ns {

fp32_t fmax(fp32_t a, fp32_t b) {
    return FP_LEQ(a, b) ? b : a;
}

fp32_t fmin(fp32_t a, fp32_t b) {
    return FP_LEQ(a, b) ? a : b;
}

}  // namespace bbox_ns

#pragma hls_design
void bbox(/* in  */ ac_channel<bbox_req_t>& bbox_req_stream,
          /* out */ ac_channel<bbox_resp_t>& bbox_resp_stream) {
#ifndef __SYNTHESIS__
    while (bbox_req_stream.available(1))
#endif
    {
        bbox_req_t bbox_req = bbox_req_stream.read();
        preprocessed_ray_t& preprocessed_ray = bbox_req.preprocessed_ray;
        bbox_t& left_bbox = bbox_req.left_bbox;
        bbox_t& right_bbox = bbox_req.right_bbox;

#ifdef DUMP_TRACE
        bbox_req_fs.write(reinterpret_cast<const char*>(&bbox_req.preprocessed_ray), sizeof(preprocessed_ray_t));
        bbox_req_fs.write(reinterpret_cast<const char*>(&bbox_req.left_bbox), sizeof(bbox_t));
        bbox_req_fs.write(reinterpret_cast<const char*>(&bbox_req.right_bbox), sizeof(bbox_t));
#endif
    
        fp32_t& left_x_a_x = preprocessed_ray.w_x.data_ac_int()[31] ? left_bbox.x_max : left_bbox.x_min;
        fp32_t& left_x_a_y = preprocessed_ray.w_y.data_ac_int()[31] ? left_bbox.y_max : left_bbox.y_min;
        fp32_t& left_x_a_z = preprocessed_ray.w_z.data_ac_int()[31] ? left_bbox.z_max : left_bbox.z_min;
        fp32_t& left_x_b_x = preprocessed_ray.w_x.data_ac_int()[31] ? left_bbox.x_min : left_bbox.x_max;
        fp32_t& left_x_b_y = preprocessed_ray.w_y.data_ac_int()[31] ? left_bbox.y_min : left_bbox.y_max;
        fp32_t& left_x_b_z = preprocessed_ray.w_z.data_ac_int()[31] ? left_bbox.z_min : left_bbox.z_max;

        fp32_t left_entry_x = FP_ADD(FP_MUL(preprocessed_ray.w_x, left_x_a_x), preprocessed_ray.b_x);
        fp32_t left_entry_y = FP_ADD(FP_MUL(preprocessed_ray.w_y, left_x_a_y), preprocessed_ray.b_y);
        fp32_t left_entry_z = FP_ADD(FP_MUL(preprocessed_ray.w_z, left_x_a_z), preprocessed_ray.b_z);
        fp32_t left_exit_x  = FP_ADD(FP_MUL(preprocessed_ray.w_x, left_x_b_x), preprocessed_ray.b_x);
        fp32_t left_exit_y  = FP_ADD(FP_MUL(preprocessed_ray.w_y, left_x_b_y), preprocessed_ray.b_y);
        fp32_t left_exit_z  = FP_ADD(FP_MUL(preprocessed_ray.w_z, left_x_b_z), preprocessed_ray.b_z);

        fp32_t left_entry = bbox_ns::fmax(bbox_ns::fmax(left_entry_x, left_entry_y), bbox_ns::fmax(left_entry_z, preprocessed_ray.tmin));
        fp32_t left_exit  = bbox_ns::fmin(bbox_ns::fmin(left_exit_x,  left_exit_y ), bbox_ns::fmin(left_exit_z,  preprocessed_ray.tmax));
        bool left_hit = FP_LEQ(left_entry, left_exit);

        fp32_t& right_x_a_x = preprocessed_ray.w_x.data_ac_int()[31] ? right_bbox.x_max : right_bbox.x_min;
        fp32_t& right_x_a_y = preprocessed_ray.w_y.data_ac_int()[31] ? right_bbox.y_max : right_bbox.y_min;
        fp32_t& right_x_a_z = preprocessed_ray.w_z.data_ac_int()[31] ? right_bbox.z_max : right_bbox.z_min;
        fp32_t& right_x_b_x = preprocessed_ray.w_x.data_ac_int()[31] ? right_bbox.x_min : right_bbox.x_max;
        fp32_t& right_x_b_y = preprocessed_ray.w_y.data_ac_int()[31] ? right_bbox.y_min : right_bbox.y_max;
        fp32_t& right_x_b_z = preprocessed_ray.w_z.data_ac_int()[31] ? right_bbox.z_min : right_bbox.z_max;

        fp32_t right_entry_x = FP_ADD(FP_MUL(preprocessed_ray.w_x, right_x_a_x), preprocessed_ray.b_x);
        fp32_t right_entry_y = FP_ADD(FP_MUL(preprocessed_ray.w_y, right_x_a_y), preprocessed_ray.b_y);
        fp32_t right_entry_z = FP_ADD(FP_MUL(preprocessed_ray.w_z, right_x_a_z), preprocessed_ray.b_z);
        fp32_t right_exit_x  = FP_ADD(FP_MUL(preprocessed_ray.w_x, right_x_b_x), preprocessed_ray.b_x);
        fp32_t right_exit_y  = FP_ADD(FP_MUL(preprocessed_ray.w_y, right_x_b_y), preprocessed_ray.b_y);
        fp32_t right_exit_z  = FP_ADD(FP_MUL(preprocessed_ray.w_z, right_x_b_z), preprocessed_ray.b_z);

        fp32_t right_entry = bbox_ns::fmax(bbox_ns::fmax(right_entry_x, right_entry_y), bbox_ns::fmax(right_entry_z, preprocessed_ray.tmin));
        fp32_t right_exit  = bbox_ns::fmin(bbox_ns::fmin(right_exit_x,  right_exit_y ), bbox_ns::fmin(right_exit_z,  preprocessed_ray.tmax));
        bool right_hit = FP_LEQ(right_entry, right_exit);

        bool left_first = FP_LEQ(left_entry, right_entry); 

        bbox_resp_t bbox_resp;
        bbox_resp.rid = bbox_req.rid;
        bbox_resp.left_hit = left_hit;
        bbox_resp.right_hit = right_hit;
        bbox_resp.left_first = left_first;

#ifdef DUMP_TRACE
        bbox_resp_fs.write(reinterpret_cast<const char*>(&bbox_resp.left_hit), sizeof(bool));
        bbox_resp_fs.write(reinterpret_cast<const char*>(&bbox_resp.right_hit), sizeof(bool));
        bbox_resp_fs.write(reinterpret_cast<const char*>(&bbox_resp.left_first), sizeof(bool));
#endif

        bbox_resp_stream.write(bbox_resp);
    }
}
