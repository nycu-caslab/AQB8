#include <vector>
#include <ac_channel.h>
#include "../include/datatypes.h"

void sim_ist_mem(/* in  */ ac_channel<ist_mem_req_t>& ist_mem_req_stream,
                 /* out */ ac_channel<ist_mem_resp_t>& ist_mem_resp_stream,
                 /* out */ trig_t trig_[TRIG_SRAM_DEPTH]) {
    static bool initialized = false;
    static std::vector<trig_t> trig_mem;
    if (!initialized) {
        initialized = true;
        std::ifstream trig_fs("../data/trig.bin", std::ios::binary);
        assert(trig_fs.good());
        for (trig_t trig; trig_fs.read((char*)(&trig), sizeof(trig_t)); )
            trig_mem.push_back(trig);
    }
    
    while (ist_mem_req_stream.available(1)) {
        ist_mem_req_t ist_mem_req = ist_mem_req_stream.read();
        assert(ist_mem_req.num_trigs > 0);
#ifdef DUMP_TRACE
        trace_fs << "IST " << ist_mem_req.num_trigs << " " << ist_mem_req.trig_idx << "\n";
#endif

        for (int i = 0; i < ist_mem_req.num_trigs; i++)
            trig_[i * NUM_CONCURRENT_RAYS + ist_mem_req.rid] = trig_mem[ist_mem_req.trig_idx + i];
        ist_mem_resp_t ist_mem_resp = {
            .rid = ist_mem_req.rid
        };
        ist_mem_resp_stream.write(ist_mem_resp);
    }
}
