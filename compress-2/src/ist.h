#include <ccs_dw_fp_lib.h>
#include <ac_channel.h>
#include "../include/fp_cmp.h"
#include "../include/datatypes.h"

#ifdef DUMP_TRACE
#include <fstream>
std::ofstream ist_req_fs("ist_req.bin", std::ios::binary);
std::ofstream ist_resp_fs("ist_resp.bin", std::ios::binary);
#endif

#pragma hls_design
void ist(/* in  */ ac_channel<ist_req_t>& ist_req_stream,
         /* out */ ac_channel<ist_resp_t>& ist_resp_stream) {
#ifndef __SYNTHESIS__
    while (ist_req_stream.available(1))
#endif
    {
        ist_req_t ist_req = ist_req_stream.read();
        ray_t& ray = ist_req.ray;
        trig_t& trig = ist_req.trig;

#ifdef DUMP_TRACE
        ist_req_fs.write(reinterpret_cast<const char*>(&ray), sizeof(ray_t));
        ist_req_fs.write(reinterpret_cast<const char*>(&trig), sizeof(trig_t));
#endif

        fp32_t n_x = FP_ADD(FP_MUL(trig.e1_y, trig.e2_z), -FP_MUL(trig.e1_z, trig.e2_y));
        fp32_t n_y = FP_ADD(FP_MUL(trig.e1_z, trig.e2_x), -FP_MUL(trig.e1_x, trig.e2_z));
        fp32_t n_z = FP_ADD(FP_MUL(trig.e1_x, trig.e2_y), -FP_MUL(trig.e1_y, trig.e2_x));

        fp32_t c_x = FP_ADD(trig.p0_x, -ray.origin_x);
        fp32_t c_y = FP_ADD(trig.p0_y, -ray.origin_y);
        fp32_t c_z = FP_ADD(trig.p0_z, -ray.origin_z);

        fp32_t r_x = FP_ADD(FP_MUL(ray.dir_y, c_z), -FP_MUL(ray.dir_z, c_y));
        fp32_t r_y = FP_ADD(FP_MUL(ray.dir_z, c_x), -FP_MUL(ray.dir_x, c_z));
        fp32_t r_z = FP_ADD(FP_MUL(ray.dir_x, c_y), -FP_MUL(ray.dir_y, c_x));

        fp32_t det = FP_ADD(FP_ADD(FP_MUL(ray.dir_x, n_x), FP_MUL(ray.dir_y, n_y)), FP_MUL(ray.dir_z, n_z));

        fp32_t abs_det = det.abs();
        ac_int<1, false> sgn_det = det.data_ac_int()[31];

        fp32_t uuu = FP_ADD(FP_ADD(FP_MUL(trig.e2_x, r_x), FP_MUL(trig.e2_y, r_y)), FP_MUL(trig.e2_z, r_z));
        fp32_t vvv = FP_ADD(FP_ADD(FP_MUL(trig.e1_x, r_x), FP_MUL(trig.e1_y, r_y)), FP_MUL(trig.e1_z, r_z));

        ac_int<32, false> uu_i = uuu.data_ac_int();
        ac_int<32, false> vv_i = vvv.data_ac_int();
        uu_i[31] = uu_i[31] ^ sgn_det;
        vv_i[31] = vv_i[31] ^ sgn_det;

        fp32_t uu;
        fp32_t vv;
        uu.set_data(uu_i);
        vv.set_data(vv_i);

        ist_resp_t ist_resp;
        ist_resp.rid         = ist_req.rid;
        ist_resp.intersected = false;
        ist_resp.t.set_data(ac_int<32, false>(0));
        ist_resp.u.set_data(ac_int<32, false>(0));
        ist_resp.v.set_data(ac_int<32, false>(0));
        
        if (uu.data_ac_int()[31] == 0 && vv.data_ac_int()[31] == 0 && FP_LEQ(FP_ADD(uu, vv), abs_det)) {
            fp32_t ttt = FP_ADD(FP_ADD(FP_MUL(c_x, n_x), FP_MUL(c_y, n_y)), FP_MUL(c_z, n_z));

            ac_int<32, false> tt_i = ttt.data_ac_int();
            tt_i[31] = tt_i[31] ^ sgn_det;

            fp32_t tt;
            tt.set_data(tt_i);

            if (FP_GEQ(tt, FP_MUL(abs_det, ray.tmin)) && FP_LEQ(tt, FP_MUL(abs_det, ray.tmax))) {
                fp32_t inv_abs_det = FP_RECIP(abs_det);
                ist_resp.intersected = true;
                ist_resp.t           = FP_MUL(inv_abs_det, tt);
                ist_resp.u           = FP_MUL(inv_abs_det, uu);
                ist_resp.v           = FP_MUL(inv_abs_det, vv);
            }
        }

#ifdef DUMP_TRACE
        ist_resp_fs.write(reinterpret_cast<const char*>(&ist_resp.intersected), sizeof(bool));
        ist_resp_fs.write(reinterpret_cast<const char*>(&ist_resp.t), sizeof(fp32_t));
        ist_resp_fs.write(reinterpret_cast<const char*>(&ist_resp.u), sizeof(fp32_t));
        ist_resp_fs.write(reinterpret_cast<const char*>(&ist_resp.v), sizeof(fp32_t));
#endif

        ist_resp_stream.write(ist_resp);
    }
}
