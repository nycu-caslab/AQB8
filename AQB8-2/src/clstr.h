#include <ccs_dw_fp_lib.h>
#include <ac_channel.h>
#include "../include/fp_cmp.h"
#include "../include/datatypes.h"

#ifdef DUMP_TRACE
#include <fstream>
std::ofstream clstr_req_fs("clstr_req.bin", std::ios::binary);
std::ofstream clstr_resp_fs("clstr_resp.bin", std::ios::binary);
#endif

namespace clstr_ns {

fp32_t fmax(fp32_t a, fp32_t b) {
    return FP_LEQ(a, b) ? b : a;
}

fp32_t fmin(fp32_t a, fp32_t b) {
    return FP_LEQ(a, b) ? a : b;
}

}  // namespace clstr_ns

#pragma hls_design
void clstr(/* in  */ ac_channel<clstr_req_t>& clstr_req_stream,
           /* out */ ac_channel<clstr_resp_t>& clstr_resp_stream) {
#ifndef __SYNTHESIS__
    while (clstr_req_stream.available(1))
#endif
    {
        clstr_req_t clstr_req = clstr_req_stream.read();
        preprocessed_ray_t& preprocessed_ray = clstr_req.preprocessed_ray;

#ifdef DUMP_TRACE
        clstr_req_fs.write(reinterpret_cast<const char*>(&clstr_req.preprocessed_ray), sizeof(preprocessed_ray_t));
        clstr_req_fs.write(reinterpret_cast<const char*>(&clstr_req.inv_sx_inv_sw), sizeof(fp32_t));
        clstr_req_fs.write(reinterpret_cast<const char*>(&clstr_req.x_min), sizeof(fp32_t));
        clstr_req_fs.write(reinterpret_cast<const char*>(&clstr_req.x_max), sizeof(fp32_t));
        clstr_req_fs.write(reinterpret_cast<const char*>(&clstr_req.y_min), sizeof(fp32_t));
        clstr_req_fs.write(reinterpret_cast<const char*>(&clstr_req.y_max), sizeof(fp32_t));
        clstr_req_fs.write(reinterpret_cast<const char*>(&clstr_req.z_min), sizeof(fp32_t));
        clstr_req_fs.write(reinterpret_cast<const char*>(&clstr_req.z_max), sizeof(fp32_t));
#endif

        fp32_t& x_a_x = preprocessed_ray.w_x.data_ac_int()[31] ? clstr_req.x_max : clstr_req.x_min;
        fp32_t& x_a_y = preprocessed_ray.w_y.data_ac_int()[31] ? clstr_req.y_max : clstr_req.y_min;
        fp32_t& x_a_z = preprocessed_ray.w_z.data_ac_int()[31] ? clstr_req.z_max : clstr_req.z_min;
        fp32_t& x_b_x = preprocessed_ray.w_x.data_ac_int()[31] ? clstr_req.x_min : clstr_req.x_max;
        fp32_t& x_b_y = preprocessed_ray.w_y.data_ac_int()[31] ? clstr_req.y_min : clstr_req.y_max;
        fp32_t& x_b_z = preprocessed_ray.w_z.data_ac_int()[31] ? clstr_req.z_min : clstr_req.z_max;

        fp32_t entry_x = FP_ADD(FP_MUL(preprocessed_ray.w_x, x_a_x), preprocessed_ray.b_x);
        fp32_t entry_y = FP_ADD(FP_MUL(preprocessed_ray.w_y, x_a_y), preprocessed_ray.b_y);
        fp32_t entry_z = FP_ADD(FP_MUL(preprocessed_ray.w_z, x_a_z), preprocessed_ray.b_z);
        fp32_t exit_x  = FP_ADD(FP_MUL(preprocessed_ray.w_x, x_b_x), preprocessed_ray.b_x);
        fp32_t exit_y  = FP_ADD(FP_MUL(preprocessed_ray.w_y, x_b_y), preprocessed_ray.b_y);
        fp32_t exit_z  = FP_ADD(FP_MUL(preprocessed_ray.w_z, x_b_z), preprocessed_ray.b_z);

        fp32_t entry = clstr_ns::fmax(clstr_ns::fmax(entry_x, entry_y), clstr_ns::fmax(entry_z, preprocessed_ray.tmin));
        fp32_t exit  = clstr_ns::fmin(clstr_ns::fmin(exit_x,  exit_y),  clstr_ns::fmin(exit_z,  preprocessed_ray.tmax));
        bool hit = FP_LEQ(entry, exit);

        clstr_resp_t clstr_resp;
        clstr_resp.rid = clstr_req.rid;

        if (hit) {
            fp32_t qb_l_x_float = FP_MUL(FP_ADD(FP_ADD(preprocessed_ray.b_x, -entry), FP_MUL(clstr_req.x_min, preprocessed_ray.w_x)), clstr_req.inv_sx_inv_sw);
            fp32_t qb_l_y_float = FP_MUL(FP_ADD(FP_ADD(preprocessed_ray.b_y, -entry), FP_MUL(clstr_req.y_min, preprocessed_ray.w_y)), clstr_req.inv_sx_inv_sw);
            fp32_t qb_l_z_float = FP_MUL(FP_ADD(FP_ADD(preprocessed_ray.b_z, -entry), FP_MUL(clstr_req.z_min, preprocessed_ray.w_z)), clstr_req.inv_sx_inv_sw);
            fp32_t qy_max_float = FP_MUL(FP_ADD(preprocessed_ray.tmax, -entry), clstr_req.inv_sx_inv_sw);
            ac_int<32, true> qb_l_x = FP_FLT2I(qb_l_x_float);
            ac_int<32, true> qb_l_y = FP_FLT2I(qb_l_y_float);
            ac_int<32, true> qb_l_z = FP_FLT2I(qb_l_z_float);
            ac_int<32, true> qy_max = FP_FLT2I(qy_max_float);

            clstr_resp.intersected = true;
            clstr_resp.inv_sx_inv_sw = clstr_req.inv_sx_inv_sw;
            clstr_resp.y_ref = entry;
            clstr_resp.qb_l_x = qb_l_x;
            clstr_resp.qb_l_y = qb_l_y;
            clstr_resp.qb_l_z = qb_l_z;
            clstr_resp.qy_max = qy_max;
        } else {
            clstr_resp.intersected = false;
            clstr_resp.inv_sx_inv_sw.set_data(ac_int<32, false>(0));
            clstr_resp.y_ref.set_data(ac_int<32, false>(0));
            clstr_resp.qb_l_x = 0;
            clstr_resp.qb_l_y = 0;
            clstr_resp.qb_l_z = 0;
            clstr_resp.qy_max = 0;
        }

#ifdef DUMP_TRACE
        clstr_resp_fs.write(reinterpret_cast<const char*>(&clstr_resp.intersected), sizeof(bool));
        clstr_resp_fs.write(reinterpret_cast<const char*>(&clstr_resp.inv_sx_inv_sw), sizeof(fp32_t));
        clstr_resp_fs.write(reinterpret_cast<const char*>(&clstr_resp.y_ref), sizeof(fp32_t));
        clstr_resp_fs.write(reinterpret_cast<const char*>(&clstr_resp.qb_l_x), 4);
        clstr_resp_fs.write(reinterpret_cast<const char*>(&clstr_resp.qb_l_y), 4);
        clstr_resp_fs.write(reinterpret_cast<const char*>(&clstr_resp.qb_l_z), 4);
        clstr_resp_fs.write(reinterpret_cast<const char*>(&clstr_resp.qy_max), 4);
#endif

        clstr_resp_stream.write(clstr_resp);
    }
}
