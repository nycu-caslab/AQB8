`timescale 1ns / 1ps
`include "../include/datatypes.svh"

module sim_ist_mem (
    input                                clk,
    input                                arst_n,

    input                                ist_mem_req_stream_empty_n,
    output                               ist_mem_req_stream_read,
    input     [  `IST_MEM_REQ_WIDTH-1:0] ist_mem_req_stream_dout,

    input                                ist_mem_resp_stream_full_n,
    output                               ist_mem_resp_stream_write,
    output    [ `IST_MEM_RESP_WIDTH-1:0] ist_mem_resp_stream_din,

    ref logic [         `TRIG_WIDTH-1:0] trig_sram [0:`TRIG_SRAM_DEPTH-1]
);

//////////////////////////////////////////////////

logic stall;

logic                        s1_valid;
logic [      `RID_WIDTH-1:0] s1_rid;
logic [  `FIELD_B_WIDTH-1:0] s1_num_trigs;
logic [`CHILD_IDX_WIDTH-1:0] s1_trig_idx;

logic                  s2_valid;
logic [`RID_WIDTH-1:0] s2_rid;

//////////////////////////////////////////////////

assign ist_mem_req_stream_read = (~stall);
assign ist_mem_resp_stream_write = s2_valid;
assign ist_mem_resp_stream_din = s2_rid;

assign stall = s2_valid && (~ist_mem_resp_stream_full_n);

assign s1_valid = ist_mem_req_stream_empty_n;
assign {s1_trig_idx, s1_num_trigs, s1_rid} = ist_mem_req_stream_dout;

always @(posedge clk or negedge arst_n) begin
    if (~arst_n) begin
        s2_valid <= 0;
        s2_rid   <= 0;
    end else if (~stall) begin
        s2_valid <= s1_valid;
        s2_rid   <= s1_rid;
    end
end

// verilator lint_off BLKSEQ
// verilator lint_off UNUSEDLOOP
always @(posedge clk) begin
    if (s1_valid && (~stall)) begin
        assert(s1_num_trigs > 0);
        for (int i = 0; i < s1_num_trigs; i++)
            trig_sram[{i[`FIELD_B_WIDTH-1:0], s1_rid[`TID_WIDTH+:`CID_WIDTH]}] = trig_[s1_trig_idx + i[`CHILD_IDX_WIDTH-1:0]];
    end
end
// verilator lint_on BLKSEQ
// verilator lint_on UNUSEDLOOP

`ifdef DUMP_TRACE
always @(posedge clk) begin
    if (ist_mem_req_stream_empty_n && ist_mem_req_stream_read)
        $display("IST %1d %1d", s1_num_trigs, s1_trig_idx);
end
`endif

endmodule
