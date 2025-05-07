`timescale 1ns / 1ps

module sram_1r1w #(
    parameter  DATA_WIDTH = 8,
    parameter  DEPTH      = 2,
    localparam ADDR_WIDTH = $clog2(DEPTH)
)(
    clk, arst_n, wr, wr_addr, wr_din, rd, rd_addr, rd_dout
);

input                   clk;
input                   arst_n;

input                   wr;
input  [ADDR_WIDTH-1:0] wr_addr;
input  [DATA_WIDTH-1:0] wr_din;

input                   rd;
input  [ADDR_WIDTH-1:0] rd_addr;
output [DATA_WIDTH-1:0] rd_dout;

// synopsys translate_off

reg [DATA_WIDTH-1:0] rd_dout;
reg [DATA_WIDTH-1:0] mem [0:DEPTH-1];

always @(posedge clk or negedge arst_n) begin
    if (~arst_n)
        rd_dout <= 0;
    else if (rd)
        rd_dout <= mem[rd_addr];
end

always @(posedge clk or negedge arst_n) begin
    if (~arst_n) begin
        for (int i = 0; i < DEPTH; i = i + 1)
            mem[i] <= 0;
    end else if (wr) begin
        mem[wr_addr] <= wr_din;
    end
end

//////////////////////////////////////////////////

int wr_cnt;
int rd_cnt;

always @(posedge clk or negedge arst_n) begin
    if (~arst_n)
        wr_cnt <= 0;
    else if (wr)
        wr_cnt <= wr_cnt + 1;
end

always @(posedge clk or negedge arst_n) begin
    if (~arst_n)
        rd_cnt <= 0;
    else if (rd)
        rd_cnt <= rd_cnt + 1;
end

final begin
    $display("[%m] wr_cnt: %1d", wr_cnt);
    $display("[%m] rd_cnt: %1d", rd_cnt);
end

// synopsys translate_on

endmodule
