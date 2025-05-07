#include <ac_channel.h>
#include "../include/datatypes.h"

#ifdef DUMP_TRACE
#include <fstream>
std::ofstream bbox_req_fs("bbox_req.bin", std::ios::binary);
std::ofstream bbox_resp_fs("bbox_resp.bin", std::ios::binary);
#endif

namespace bbox_ns {

ac_int<32, true> max(ac_int<32, true> a, ac_int<32, true> b) {
    return a <= b ? b : a;
}

ac_int<32, true> min(ac_int<32, true> a, ac_int<32, true> b) {
    return a <= b ? a : b;
}

ac_int<32, true> quant_calc(bool& iw,
                            ac_int<7, false>& qw,
                            ac_int<5, false>& rw,
                            ac_int<8, false>& qx,
                            ac_int<32, true>& qb) {
    ui8_t qw_ext;
    qw_ext.set_slc(7, ac_int<1, false>(1));
    qw_ext.set_slc(0, qw);
    ac_int<32, true> ret = qw_ext * qx;
    ret <<= rw;
    ret = (iw ? ac_int<32, true>(-ret) : ret);
    ret += qb;
    return ret;
}

}  // namespace bbox_ns

#pragma hls_design
void bbox(/* in  */ ac_channel<bbox_req_t>& bbox_req_stream,
          /* out */ ac_channel<bbox_resp_t>& bbox_resp_stream) {
    using bbox_ns::quant_calc;

#ifndef __SYNTHESIS__
    while (bbox_req_stream.available(1))
#endif
    {
        bbox_req_t bbox_req = bbox_req_stream.read();
        quant_ray_t& quant_ray = bbox_req.quant_ray;
        bbox_t& left_bbox = bbox_req.left_bbox;
        bbox_t& right_bbox = bbox_req.right_bbox;

#ifdef DUMP_TRACE
        int dump[31] = {
            quant_ray.iw_x,
            quant_ray.iw_y,
            quant_ray.iw_z,
            quant_ray.qw_l_x.to_int(),
            quant_ray.qw_l_y.to_int(),
            quant_ray.qw_l_z.to_int(),
            quant_ray.qw_h_x.to_int(),
            quant_ray.qw_h_y.to_int(),
            quant_ray.qw_h_z.to_int(),
            quant_ray.rw_l_x.to_int(),
            quant_ray.rw_l_y.to_int(),
            quant_ray.rw_l_z.to_int(),
            quant_ray.rw_h_x.to_int(),
            quant_ray.rw_h_y.to_int(),
            quant_ray.rw_h_z.to_int(),
            quant_ray.qb_l_x.to_int(),
            quant_ray.qb_l_y.to_int(),
            quant_ray.qb_l_z.to_int(),
            quant_ray.qy_max.to_int(),
            left_bbox.x_min.to_int(),
            left_bbox.x_max.to_int(),
            left_bbox.y_min.to_int(),
            left_bbox.y_max.to_int(),
            left_bbox.z_min.to_int(),
            left_bbox.z_max.to_int(),
            right_bbox.x_min.to_int(),
            right_bbox.x_max.to_int(),
            right_bbox.y_min.to_int(),
            right_bbox.y_max.to_int(),
            right_bbox.z_min.to_int(),
            right_bbox.z_max.to_int()
        };
        bbox_req_fs.write(reinterpret_cast<const char*>(dump), 31*sizeof(int));
#endif

        i32_t qb_h_x = quant_ray.qb_l_x + 1;
        i32_t qb_h_y = quant_ray.qb_l_y + 1;
        i32_t qb_h_z = quant_ray.qb_l_z + 1;

        ac_int<8, false>& left_qx_a_x = quant_ray.iw_x ? left_bbox.x_max : left_bbox.x_min;
        ac_int<8, false>& left_qx_a_y = quant_ray.iw_y ? left_bbox.y_max : left_bbox.y_min;
        ac_int<8, false>& left_qx_a_z = quant_ray.iw_z ? left_bbox.z_max : left_bbox.z_min;
        ac_int<8, false>& left_qx_b_x = quant_ray.iw_x ? left_bbox.x_min : left_bbox.x_max;
        ac_int<8, false>& left_qx_b_y = quant_ray.iw_y ? left_bbox.y_min : left_bbox.y_max;
        ac_int<8, false>& left_qx_b_z = quant_ray.iw_z ? left_bbox.z_min : left_bbox.z_max;

        ac_int<32, true> left_entry_x = quant_calc(quant_ray.iw_x, quant_ray.qw_l_x, quant_ray.rw_l_x, left_qx_a_x, quant_ray.qb_l_x);
        ac_int<32, true> left_entry_y = quant_calc(quant_ray.iw_y, quant_ray.qw_l_y, quant_ray.rw_l_y, left_qx_a_y, quant_ray.qb_l_y);
        ac_int<32, true> left_entry_z = quant_calc(quant_ray.iw_z, quant_ray.qw_l_z, quant_ray.rw_l_z, left_qx_a_z, quant_ray.qb_l_z);
        ac_int<32, true> left_exit_x  = quant_calc(quant_ray.iw_x, quant_ray.qw_h_x, quant_ray.rw_h_x, left_qx_b_x, qb_h_x);
        ac_int<32, true> left_exit_y  = quant_calc(quant_ray.iw_y, quant_ray.qw_h_y, quant_ray.rw_h_y, left_qx_b_y, qb_h_y);
        ac_int<32, true> left_exit_z  = quant_calc(quant_ray.iw_z, quant_ray.qw_h_z, quant_ray.rw_h_z, left_qx_b_z, qb_h_z);

        ac_int<32, true> left_entry = bbox_ns::max(bbox_ns::max(left_entry_x, left_entry_y), bbox_ns::max(left_entry_z, 0));
        ac_int<32, true> left_exit  = bbox_ns::min(bbox_ns::min(left_exit_x,  left_exit_y ), bbox_ns::min(left_exit_z,  quant_ray.qy_max));
        bool left_hit = left_entry <= left_exit;

        ac_int<8, false>& right_qx_a_x = quant_ray.iw_x ? right_bbox.x_max : right_bbox.x_min;
        ac_int<8, false>& right_qx_a_y = quant_ray.iw_y ? right_bbox.y_max : right_bbox.y_min;
        ac_int<8, false>& right_qx_a_z = quant_ray.iw_z ? right_bbox.z_max : right_bbox.z_min;
        ac_int<8, false>& right_qx_b_x = quant_ray.iw_x ? right_bbox.x_min : right_bbox.x_max;
        ac_int<8, false>& right_qx_b_y = quant_ray.iw_y ? right_bbox.y_min : right_bbox.y_max;
        ac_int<8, false>& right_qx_b_z = quant_ray.iw_z ? right_bbox.z_min : right_bbox.z_max;

        ac_int<32, true> right_entry_x = quant_calc(quant_ray.iw_x, quant_ray.qw_l_x, quant_ray.rw_l_x, right_qx_a_x, quant_ray.qb_l_x);
        ac_int<32, true> right_entry_y = quant_calc(quant_ray.iw_y, quant_ray.qw_l_y, quant_ray.rw_l_y, right_qx_a_y, quant_ray.qb_l_y);
        ac_int<32, true> right_entry_z = quant_calc(quant_ray.iw_z, quant_ray.qw_l_z, quant_ray.rw_l_z, right_qx_a_z, quant_ray.qb_l_z);
        ac_int<32, true> right_exit_x  = quant_calc(quant_ray.iw_x, quant_ray.qw_h_x, quant_ray.rw_h_x, right_qx_b_x, qb_h_x);
        ac_int<32, true> right_exit_y  = quant_calc(quant_ray.iw_y, quant_ray.qw_h_y, quant_ray.rw_h_y, right_qx_b_y, qb_h_y);
        ac_int<32, true> right_exit_z  = quant_calc(quant_ray.iw_z, quant_ray.qw_h_z, quant_ray.rw_h_z, right_qx_b_z, qb_h_z);

        ac_int<32, true> right_entry = bbox_ns::max(bbox_ns::max(right_entry_x, right_entry_y), bbox_ns::max(right_entry_z, 0));
        ac_int<32, true> right_exit  = bbox_ns::min(bbox_ns::min(right_exit_x,  right_exit_y ), bbox_ns::min(right_exit_z,  quant_ray.qy_max));
        bool right_hit = right_entry <= right_exit;

        bool left_first = left_entry <= right_entry; 

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
