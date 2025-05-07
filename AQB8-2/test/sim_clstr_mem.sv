`timescale 1ns / 1ps
`include "../include/datatypes.svh"

module sim_clstr_mem (
    input                              clk,
    input                              arst_n,

    input                              clstr_mem_req_stream_empty_n,
    output                             clstr_mem_req_stream_read,
    input  [ `CLSTR_MEM_REQ_WIDTH-1:0] clstr_mem_req_stream_dout,

    input                              clstr_mem_resp_stream_full_n,
    output                             clstr_mem_resp_stream_write,
    output [`CLSTR_MEM_RESP_WIDTH-1:0] clstr_mem_resp_stream_din
);

//////////////////////////////////////////////////

logic stall;

logic                          s1_valid;
logic [        `RID_WIDTH-1:0] s1_rid;
logic [`CLUSTER_IDX_WIDTH-1:0] s1_cluster_idx;

logic                      s2_valid;
logic [    `RID_WIDTH-1:0] s2_rid;
logic [`CLUSTER_WIDTH-1:0] s2_cluster;

//////////////////////////////////////////////////

assign clstr_mem_req_stream_read = (~stall);
assign clstr_mem_resp_stream_write = s2_valid;
assign clstr_mem_resp_stream_din = {s2_cluster, s2_rid};

assign stall = s2_valid && (~clstr_mem_resp_stream_full_n);

assign s1_valid = clstr_mem_req_stream_empty_n;
assign {s1_cluster_idx, s1_rid} = clstr_mem_req_stream_dout;

always @(posedge clk or negedge arst_n) begin
    if (~arst_n) begin
        s2_valid   <= 0;
        s2_rid     <= 0;
        s2_cluster <= 0;
    end else if (~stall) begin
        s2_valid   <= s1_valid;
        s2_rid     <= s1_rid;
        s2_cluster <= cluster_[s1_cluster_idx];
    end
end

`ifdef DUMP_TRACE
always @(posedge clk) begin
    if (clstr_mem_req_stream_empty_n && clstr_mem_req_stream_read)
        $display("CLSTR %1d", s1_cluster_idx);
end
`endif

endmodule
