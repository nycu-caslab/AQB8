`timescale 1ns / 1ps
`include "../include/datatypes.svh"

module sim_bbox_mem (
    input                             clk,
    input                             arst_n,

    input                             bbox_mem_req_stream_empty_n,
    output                            bbox_mem_req_stream_read,
    input  [ `BBOX_MEM_REQ_WIDTH-1:0] bbox_mem_req_stream_dout,

    input                             bbox_mem_resp_stream_full_n,
    output                            bbox_mem_resp_stream_write,
    output [`BBOX_MEM_RESP_WIDTH-1:0] bbox_mem_resp_stream_din
);

//////////////////////////////////////////////////

logic stall;

logic                        s1_valid;
logic [      `RID_WIDTH-1:0] s1_rid;
logic [`CHILD_IDX_WIDTH-1:0] s1_nbp_idx;

logic                  s2_valid;
logic [`RID_WIDTH-1:0] s2_rid;
logic [`NBP_WIDTH-1:0] s2_nbp;

//////////////////////////////////////////////////

assign bbox_mem_req_stream_read = (~stall);
assign bbox_mem_resp_stream_write = s2_valid;
assign bbox_mem_resp_stream_din = {s2_nbp, s2_rid};

assign stall = s2_valid && (~bbox_mem_resp_stream_full_n);

assign s1_valid = bbox_mem_req_stream_empty_n;
assign {s1_nbp_idx, s1_rid} = bbox_mem_req_stream_dout;

always @(posedge clk or negedge arst_n) begin
    if (~arst_n) begin
        s2_valid <= 0;
        s2_rid   <= 0;
        s2_nbp   <= 0;
    end else if (~stall) begin
        s2_valid <= s1_valid;
        s2_rid   <= s1_rid;
        s2_nbp   <= nbp_[s1_nbp_idx];
    end
end

`ifdef DUMP_TRACE
always @(posedge clk) begin
    if (bbox_mem_req_stream_empty_n && bbox_mem_req_stream_read)
        $display("BBOX %1d", s1_nbp_idx);
end
`endif

endmodule
