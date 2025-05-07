`timescale 1ns / 1ps

module fifo #(
    parameter DATA_WIDTH = 8
)(
    clk, arst_n, full_n, write, din, empty_n, read, dout
);

input                   clk;
input                   arst_n;

output                  full_n;
input                   write;
input  [DATA_WIDTH-1:0] din;

output                  empty_n;
input                   read;
output [DATA_WIDTH-1:0] dout;

logic                   empty_n;
logic  [DATA_WIDTH-1:0] dout;

//////////////////////////////////////////////////

logic [DATA_WIDTH-1:0] data [$];
integer data_size;

assign full_n = 1'b1;

always @(*) begin
    if (data_size > 0) begin
        empty_n = 1'b1;
        dout    = data[0];
    end else begin
        empty_n = 1'b0;
        dout    = {DATA_WIDTH{1'b0}};
    end
end

// verilator lint_off BLKSEQ
always @(posedge clk or negedge arst_n) begin
    if (~arst_n) begin
        data.delete();
        data_size <= 0;
    end else begin
        automatic int push_cnt = 0;
        automatic int pop_cnt = 0;
        if (full_n & write) begin
            data.push_back(din);
            push_cnt += 1;
        end
        if (empty_n & read) begin
            void'(data.pop_front());
            pop_cnt += 1;
        end
        data_size <= data_size + push_cnt - pop_cnt;
    end 
end
// verilator lint_on BLKSEQ

//////////////////////////////////////////////////

int total_cycle_count;
int write_cycle_count;
int read_cycle_count;

always @(posedge clk or negedge arst_n) begin
    if (~arst_n)
        total_cycle_count <= 0;
    else
        total_cycle_count <= total_cycle_count + 1;
end

always @(posedge clk or negedge arst_n) begin
    if (~arst_n)
        write_cycle_count <= 0;
    else if (full_n & write)
        write_cycle_count <= write_cycle_count + 1;
end

always @(posedge clk or negedge arst_n) begin
    if (~arst_n)
        read_cycle_count <= 0;
    else if (empty_n & read)
        read_cycle_count <= read_cycle_count + 1;
end

final $display("[%m]: write: %1d/%1d", write_cycle_count, total_cycle_count);
final $display("[%m]: read: %1d/%1d", read_cycle_count, total_cycle_count);

endmodule
