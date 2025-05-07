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
        init_req_fs.write(reinterpret_cast<const char*>(&init_req.ref_bbox), sizeof(ref_bbox_t));
#endif

        fp32_t w_x = FP_RECIP(ray.dir_x);
        fp32_t w_y = FP_RECIP(ray.dir_y);
        fp32_t w_z = FP_RECIP(ray.dir_z);

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
        trv_req.all_ray.b_x      = FP_MUL(FP_ADD(init_req.ref_bbox.x_min, -ray.origin_x), w_x);
        trv_req.all_ray.b_y      = FP_MUL(FP_ADD(init_req.ref_bbox.y_min, -ray.origin_y), w_y);
        trv_req.all_ray.b_z      = FP_MUL(FP_ADD(init_req.ref_bbox.z_min, -ray.origin_z), w_z);
        trv_req.all_ray.tmin     = ray.tmin;
        trv_req.all_ray.tmax     = ray.tmax;

#ifdef DUMP_TRACE
        trv_req_fs.write(reinterpret_cast<const char*>(&trv_req.all_ray), sizeof(all_ray_t));
#endif

        trv_req_stream.write(trv_req);
    }
}
