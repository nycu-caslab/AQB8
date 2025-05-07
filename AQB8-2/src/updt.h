#include <ccs_dw_fp_lib.h>
#include <ac_channel.h>
#include "../include/datatypes.h"

#ifdef DUMP_TRACE
#include <fstream>
extern std::ofstream trace_fs;
std::ofstream updt_req_fs("updt_req.bin", std::ios::binary);
std::ofstream updt_resp_fs("updt_resp.bin", std::ios::binary);
#endif

#pragma hls_design
void updt(/* in  */ ac_channel<updt_req_t>& updt_req_stream,
          /* out */ ac_channel<updt_resp_t>& updt_resp_stream) {
#ifndef __SYNTHESIS__
    while (updt_req_stream.available(1))
#endif
    {
        updt_req_t updt_req = updt_req_stream.read();

#ifdef DUMP_TRACE
        trace_fs << "UPDT\n";
        updt_req_fs.write(reinterpret_cast<const char*>(&updt_req.inv_sx_inv_sw), sizeof(fp32_t));
        updt_req_fs.write(reinterpret_cast<const char*>(&updt_req.y_ref), sizeof(fp32_t));
        updt_req_fs.write(reinterpret_cast<const char*>(&updt_req.tmax), sizeof(fp32_t));
#endif

        fp32_t qy_max_float = FP_MUL(FP_ADD(updt_req.tmax, -updt_req.y_ref), updt_req.inv_sx_inv_sw);
        ac_int<32, true> qy_max = FP_FLT2I(qy_max_float);

        updt_resp_t updt_resp;
        updt_resp.rid = updt_req.rid;
        updt_resp.qy_max = qy_max;

#ifdef DUMP_TRACE
        int qy_max_int = updt_resp.qy_max.to_int();
        updt_resp_fs.write(reinterpret_cast<const char*>(&qy_max_int), sizeof(int));
#endif

        updt_resp_stream.write(updt_resp);
    }
}
