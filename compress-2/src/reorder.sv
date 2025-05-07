`timescale 1ns / 1ps
`include "../include/datatypes.svh"

module reorder #(
    parameter [`TID_WIDTH-1:0] TID = 0
)(
    input                        clk,
    input                        arst_n,

    input                        ray_stream_empty_n,
    output                       ray_stream_read,
    input  [     `RAY_WIDTH-1:0] ray_stream_dout,

    input                        trv_resp_stream_empty_n,
    output                       trv_resp_stream_read,
    input  [`TRV_RESP_WIDTH-1:0] trv_resp_stream_dout,

    input                        init_req_stream_full_n,
    output                       init_req_stream_write,
    output [`INIT_REQ_WIDTH-1:0] init_req_stream_din,

    input                        result_stream_full_n,
    output                       result_stream_write,
    output [  `RESULT_WIDTH-1:0] result_stream_din,

    input  [`REF_BBOX_WIDTH-1:0] gen_ref_bbox
);

`ifdef FIX_CID_TO_ZERO

//==============================================//

reg processing;

//////////////////////////////////////////////////

assign ray_stream_read       = (~processing) && ray_stream_empty_n && init_req_stream_full_n;
assign trv_resp_stream_read  = trv_resp_stream_empty_n && result_stream_full_n;
assign init_req_stream_write = ray_stream_read;
assign init_req_stream_din   = {gen_ref_bbox, ray_stream_dout, {`CID_WIDTH{1'b0}}, TID};
assign result_stream_write   = trv_resp_stream_read;
assign result_stream_din     = trv_resp_stream_dout[`TRV_RESP_WIDTH-1:`RID_WIDTH];

always @(posedge clk or negedge arst_n) begin
    if (~arst_n)
        processing <= 0;
    else if (ray_stream_read)
        processing <= 1;
    else if (result_stream_write)
        processing <= 0;
end

//==============================================//

`else  // FIX_CID_TO_ZERO

//==============================================//

genvar gi;
integer i;

//////////////////////////////////////////////////

reg                      result_stream_write_reg;

// verilator lint_off UNUSEDSIGNAL
wire [   `TID_WIDTH-1:0] trv_resp_tid;
// verilator lint_on UNUSEDSIGNAL
wire [   `CID_WIDTH-1:0] trv_resp_cid;
wire [`RESULT_WIDTH-1:0] trv_resp_result;

reg                      initializing;
reg  [   `CID_WIDTH-1:0] init_ptr;
reg  [     `CID_WIDTH:0] ray_ptr;
reg  [     `CID_WIDTH:0] result_ptr;
reg  [   `CID_WIDTH+1:0] quota;

reg  [     `CID_WIDTH:0] mapper       [  0:`NUM_CONCURRENT_RAYS-1];
reg                      result_valid [0:2*`NUM_CONCURRENT_RAYS-1];

wire [     `CID_WIDTH:0] mapper_trv_resp_cid;

// verilator lint_off UNUSEDSIGNAL
wire                     fifo_ready_full_n;
// verilator lint_on UNUSEDSIGNAL
wire                     fifo_ready_write;
wire [   `CID_WIDTH-1:0] fifo_ready_din;
wire                     fifo_ready_empty_n;
wire                     fifo_ready_read;
wire [   `CID_WIDTH-1:0] fifo_ready_dout;

wire                     sram_wr;
wire [     `CID_WIDTH:0] sram_wr_addr;
wire [`RESULT_WIDTH-1:0] sram_wr_din;
wire                     sram_rd;
wire [     `CID_WIDTH:0] sram_rd_addr;
wire [`RESULT_WIDTH-1:0] sram_rd_dout;

//////////////////////////////////////////////////

fifo #(
    .DATA_WIDTH(`CID_WIDTH)
) fifo_ready (
    .clk(clk),
    .arst_n(arst_n),
    .full_n(fifo_ready_full_n),
    .write(fifo_ready_write),
    .din(fifo_ready_din),
    .empty_n(fifo_ready_empty_n),
    .read(fifo_ready_read),
    .dout(fifo_ready_dout)
);

sram_1r1w #(
    .DATA_WIDTH(`RESULT_WIDTH),
    .DEPTH(2*`NUM_CONCURRENT_RAYS)
) sram_result (
    .clk(clk),
    .arst_n(arst_n),
    .wr(sram_wr),
    .wr_addr(sram_wr_addr),
    .wr_din(sram_wr_din),
    .rd(sram_rd),
    .rd_addr(sram_rd_addr),
    .rd_dout(sram_rd_dout)
);

//////////////////////////////////////////////////

assign ray_stream_read       = (~initializing) && (quota != 0) &&
                               fifo_ready_empty_n && ray_stream_empty_n && init_req_stream_full_n;
assign trv_resp_stream_read  = trv_resp_stream_empty_n;
assign init_req_stream_write = ray_stream_read;
assign init_req_stream_din   = {gen_ref_bbox, ray_stream_dout, fifo_ready_dout, TID};
assign result_stream_write   = result_stream_write_reg;
assign result_stream_din     = sram_rd_dout;

always @(posedge clk or negedge arst_n) begin
    if (~arst_n) begin
        result_stream_write_reg <= 0;
    end else begin
        case (result_stream_write_reg)
            0: begin
                if (result_valid[result_ptr] && result_stream_full_n)
                    result_stream_write_reg <= 1;
            end
            1: begin
                result_stream_write_reg <= 0;
            end
        endcase
    end
end

assign {trv_resp_result, trv_resp_cid, trv_resp_tid} = trv_resp_stream_dout;

always @(posedge clk or negedge arst_n) begin
    if (~arst_n)
        initializing <= 1;
    else if (&init_ptr)
        initializing <= 0;
end

always @(posedge clk or negedge arst_n) begin
    if (~arst_n)
        init_ptr <= 0;
    else
        init_ptr <= init_ptr + 1'b1;
end

always @(posedge clk or negedge arst_n) begin
    if (~arst_n)
        ray_ptr <= `NUM_CONCURRENT_RAYS;
    else if (trv_resp_stream_read)
        ray_ptr <= ray_ptr + 1'b1;
end

always @(posedge clk or negedge arst_n) begin
    if (~arst_n)
        result_ptr <= 0;
    else if (result_stream_write)
        result_ptr <= result_ptr + 1'b1;
end

always @(posedge clk or negedge arst_n) begin
    if (~arst_n) begin
        quota <= {1'b1, {`CID_WIDTH+1{1'b0}}};
    end else begin
        case ({ray_stream_read, result_stream_write})
            2'b01:   quota <= quota + 1'b1;
            2'b10:   quota <= quota - 1'b1;
            default: quota <= quota;
        endcase
    end
end

always @(posedge clk or negedge arst_n) begin
    if (~arst_n) begin
        for (i = 0; i < `NUM_CONCURRENT_RAYS; i = i + 1)
            mapper[i] <= i[`CID_WIDTH:0];
    end else if (trv_resp_stream_read) begin
        mapper[trv_resp_cid] <= ray_ptr;
    end
end

generate
    for (gi = 0; gi < 2*`NUM_CONCURRENT_RAYS; gi = gi + 1) begin
        always @(posedge clk or negedge arst_n) begin
            if (~arst_n)
                result_valid[gi] <= 0;
            else if (trv_resp_stream_read && mapper_trv_resp_cid == gi)
                result_valid[gi] <= 1;
            else if (result_stream_write && result_ptr == gi)
                result_valid[gi] <= 0;
        end
    end
endgenerate

assign mapper_trv_resp_cid = mapper[trv_resp_cid];

assign fifo_ready_write = initializing || trv_resp_stream_read;
assign fifo_ready_din   = initializing ? init_ptr : trv_resp_cid;
assign fifo_ready_read  = ray_stream_read;

assign sram_wr      = trv_resp_stream_read;
assign sram_wr_addr = mapper_trv_resp_cid;
assign sram_wr_din  = trv_resp_result;
assign sram_rd      = 1'b1;
assign sram_rd_addr = result_ptr;

//==============================================//

`endif  // FIX_CID_TO_ZERO

endmodule
