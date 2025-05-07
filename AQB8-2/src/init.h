#include <ccs_dw_fp_lib.h>
#include <ac_channel.h>
#include "../include/datatypes.h"

#ifdef DUMP_TRACE
#include <fstream>
std::ofstream init_req_fs("init_req.bin", std::ios::binary);
std::ofstream trv_req_fs("trv_req.bin", std::ios::binary);
#endif

#pragma hls_design
void init(/* in  */ ac_channel<init_req_t>& init_req_stream,
          /* out */ ac_channel<trv_req_t>& trv_req_stream) {
#ifndef __SYNTHESIS__
    while (init_req_stream.available(1))
#endif
    {
        init_req_t init_req = init_req_stream.read();
        ray_t& ray = init_req.ray;

#ifdef DUMP_TRACE
        init_req_fs.write(reinterpret_cast<const char*>(&ray), sizeof(ray_t));
#endif

        fp32_t w_x = FP_RECIP(ray.dir_x);
        fp32_t w_y = FP_RECIP(ray.dir_y);
        fp32_t w_z = FP_RECIP(ray.dir_z);

        ac_int<32, false> w_x_uint = w_x.data_ac_int();
        ac_int<32, false> w_y_uint = w_y.data_ac_int();
        ac_int<32, false> w_z_uint = w_z.data_ac_int();

        bool sign_x = w_x_uint.slc<1>(31);
        bool sign_y = w_y_uint.slc<1>(31);
        bool sign_z = w_z_uint.slc<1>(31);
        ac_int<8, false> exponent_x  = w_x_uint.slc<8>(23);
        ac_int<8, false> exponent_y  = w_y_uint.slc<8>(23);
        ac_int<8, false> exponent_z  = w_z_uint.slc<8>(23);
        ac_int<23, false> mantissa_x = w_x_uint.slc<23>(0);
        ac_int<23, false> mantissa_y = w_y_uint.slc<23>(0);
        ac_int<23, false> mantissa_z = w_z_uint.slc<23>(0);
        ac_int<5, false> near_exponent_x = exponent_x - 127;
        ac_int<5, false> near_exponent_y = exponent_y - 127;
        ac_int<5, false> near_exponent_z = exponent_z - 127;
        ac_int<7, false> near_mantissa_x = mantissa_x.slc<7>(16);
        ac_int<7, false> near_mantissa_y = mantissa_y.slc<7>(16);
        ac_int<7, false> near_mantissa_z = mantissa_z.slc<7>(16);
        ac_int<12, false> near_x;
        near_x.set_slc(0, near_mantissa_x);
        near_x.set_slc(7, near_exponent_x);
        ac_int<12, false> near_y;
        near_y.set_slc(0, near_mantissa_y);
        near_y.set_slc(7, near_exponent_y);
        ac_int<12, false> near_z;
        near_z.set_slc(0, near_mantissa_z);
        near_z.set_slc(7, near_exponent_z);
        ac_int<12, false> far_x = near_x + 1;
        ac_int<12, false> far_y = near_y + 1;
        ac_int<12, false> far_z = near_z + 1;
        ac_int<5, false> far_exponent_x = far_x.slc<5>(7);
        ac_int<5, false> far_exponent_y = far_y.slc<5>(7);
        ac_int<5, false> far_exponent_z = far_z.slc<5>(7);
        ac_int<7, false> far_mantissa_x = far_x.slc<7>(0);
        ac_int<7, false> far_mantissa_y = far_y.slc<7>(0);
        ac_int<7, false> far_mantissa_z = far_z.slc<7>(0);

        trv_req_t trv_req;
        trv_req.rid              = init_req.rid;
        trv_req.all_ray.origin_x = ray.origin_x;
        trv_req.all_ray.origin_y = ray.origin_y;
        trv_req.all_ray.origin_z = ray.origin_z;
        trv_req.all_ray.dir_x    = ray.dir_x;
        trv_req.all_ray.dir_y    = ray.dir_y;
        trv_req.all_ray.dir_z    = ray.dir_z;
        trv_req.all_ray.w_x      = w_x;
        trv_req.all_ray.w_y      = w_y;
        trv_req.all_ray.w_z      = w_z;
        trv_req.all_ray.b_x      = FP_MUL(-ray.origin_x, w_x);
        trv_req.all_ray.b_y      = FP_MUL(-ray.origin_y, w_y);
        trv_req.all_ray.b_z      = FP_MUL(-ray.origin_z, w_z);
        trv_req.all_ray.qw_l_x   = sign_x ? far_mantissa_x  : near_mantissa_x;
        trv_req.all_ray.qw_l_y   = sign_y ? far_mantissa_y  : near_mantissa_y;
        trv_req.all_ray.qw_l_z   = sign_z ? far_mantissa_z  : near_mantissa_z;
        trv_req.all_ray.qw_h_x   = sign_x ? near_mantissa_x : far_mantissa_x;
        trv_req.all_ray.qw_h_y   = sign_y ? near_mantissa_y : far_mantissa_y;
        trv_req.all_ray.qw_h_z   = sign_z ? near_mantissa_z : far_mantissa_z;
        trv_req.all_ray.rw_l_x   = sign_x ? far_exponent_x  : near_exponent_x;
        trv_req.all_ray.rw_l_y   = sign_y ? far_exponent_y  : near_exponent_y;
        trv_req.all_ray.rw_l_z   = sign_z ? far_exponent_z  : near_exponent_z;
        trv_req.all_ray.rw_h_x   = sign_x ? near_exponent_x : far_exponent_x;
        trv_req.all_ray.rw_h_y   = sign_y ? near_exponent_y : far_exponent_y;
        trv_req.all_ray.rw_h_z   = sign_z ? near_exponent_z : far_exponent_z;
        trv_req.all_ray.tmin     = ray.tmin;
        trv_req.all_ray.tmax     = ray.tmax;

#ifdef DUMP_TRACE
        int dump[12] = {
            trv_req.all_ray.qw_l_x.to_int(),
            trv_req.all_ray.qw_l_y.to_int(),
            trv_req.all_ray.qw_l_z.to_int(),
            trv_req.all_ray.qw_h_x.to_int(),
            trv_req.all_ray.qw_h_y.to_int(),
            trv_req.all_ray.qw_h_z.to_int(),
            trv_req.all_ray.rw_l_x.to_int(),
            trv_req.all_ray.rw_l_y.to_int(),
            trv_req.all_ray.rw_l_z.to_int(),
            trv_req.all_ray.rw_h_x.to_int(),
            trv_req.all_ray.rw_h_y.to_int(),
            trv_req.all_ray.rw_h_z.to_int()
        };
        trv_req_fs.write(reinterpret_cast<const char*>(&trv_req.all_ray.origin_x), sizeof(fp32_t));
        trv_req_fs.write(reinterpret_cast<const char*>(&trv_req.all_ray.origin_y), sizeof(fp32_t));
        trv_req_fs.write(reinterpret_cast<const char*>(&trv_req.all_ray.origin_z), sizeof(fp32_t));
        trv_req_fs.write(reinterpret_cast<const char*>(&trv_req.all_ray.dir_x), sizeof(fp32_t));
        trv_req_fs.write(reinterpret_cast<const char*>(&trv_req.all_ray.dir_y), sizeof(fp32_t));
        trv_req_fs.write(reinterpret_cast<const char*>(&trv_req.all_ray.dir_z), sizeof(fp32_t));
        trv_req_fs.write(reinterpret_cast<const char*>(&trv_req.all_ray.w_x), sizeof(fp32_t));
        trv_req_fs.write(reinterpret_cast<const char*>(&trv_req.all_ray.w_y), sizeof(fp32_t));
        trv_req_fs.write(reinterpret_cast<const char*>(&trv_req.all_ray.w_z), sizeof(fp32_t));
        trv_req_fs.write(reinterpret_cast<const char*>(&trv_req.all_ray.b_x), sizeof(fp32_t));
        trv_req_fs.write(reinterpret_cast<const char*>(&trv_req.all_ray.b_y), sizeof(fp32_t));
        trv_req_fs.write(reinterpret_cast<const char*>(&trv_req.all_ray.b_z), sizeof(fp32_t));
        trv_req_fs.write(reinterpret_cast<const char*>(dump), 12*sizeof(int));
        trv_req_fs.write(reinterpret_cast<const char*>(&trv_req.all_ray.tmin), sizeof(fp32_t));
        trv_req_fs.write(reinterpret_cast<const char*>(&trv_req.all_ray.tmax), sizeof(fp32_t));
#endif

        trv_req_stream.write(trv_req);
    }
}
