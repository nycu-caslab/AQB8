#include "../src/init.h"
#include "../src/clstr.h"
#include "../src/updt.h"
#include "../src/bbox.h"
#include "../src/ist.h"

#pragma hls_design top
void rtcore(/* in  */ ac_channel<init_req_t>& init_req_stream,
            /* in  */ ac_channel<clstr_req_t>& clstr_req_stream,
            /* in  */ ac_channel<updt_req_t>& updt_req_stream,
            /* in  */ ac_channel<bbox_req_t>& bbox_req_stream,
            /* in  */ ac_channel<ist_req_t>& ist_req_stream,
            /* out */ ac_channel<trv_req_t>& trv_req_stream,
            /* out */ ac_channel<clstr_resp_t>& clstr_resp_stream,
            /* out */ ac_channel<updt_resp_t>& updt_resp_stream,
            /* out */ ac_channel<bbox_resp_t>& bbox_resp_stream,
            /* out */ ac_channel<ist_resp_t>& ist_resp_stream) {
    init(init_req_stream, trv_req_stream);
    clstr(clstr_req_stream, clstr_resp_stream);
    updt(updt_req_stream, updt_resp_stream);
    bbox(bbox_req_stream, bbox_resp_stream);
    ist(ist_req_stream, ist_resp_stream);
}
