`timescale 1ns / 1ps

module fifo_fw #(
    parameter M = 3,
    parameter N = 2,
    parameter DATA_WIDTH = 32
)(
    clk, arst_n, full_n, write, din, empty_n, read, dout
);

input                   clk;
input                   arst_n;

output                  full_n  [0:M-1];
input                   write   [0:M-1];
input  [DATA_WIDTH-1:0] din     [0:M-1];

output                  empty_n [0:N-1];
input                   read    [0:N-1];
output [DATA_WIDTH-1:0] dout    [0:N-1];

logic                   empty_n [0:N-1];
logic  [DATA_WIDTH-1:0] dout    [0:N-1];

//////////////////////////////////////////////////

logic [DATA_WIDTH-1:0] data [$];
integer data_size;

generate
    for (genvar gi = 0; gi < M; gi++) begin : gen_write
        assign full_n[gi] = 1'b1;
    end
endgenerate

generate
    for (genvar gi = 0; gi < N; gi++) begin : gen_read
        always @(*) begin
            if (data_size >= gi + 1) begin
                empty_n[gi] = 1'b1;
                dout   [gi] = data[gi];
            end else begin
                empty_n[gi] = 1'b0;
                dout   [gi] = {DATA_WIDTH{1'b0}};
            end
        end
    end
endgenerate

// verilator lint_off BLKSEQ
always @(posedge clk or negedge arst_n) begin
    if (~arst_n) begin
        data.delete();
        data_size <= 0;
    end else begin
        automatic int push_cnt = 0;
        automatic int pop_cnt = 0;
        for (int i = 0; i < M; i++) begin
            if (full_n[i] & write[i]) begin
                data.push_back(din[i]);
                push_cnt += 1;
            end
        end
        for (int i = 0; i < N; i++) begin
            if (empty_n[i]) begin
                void'(data.pop_front());
                pop_cnt += 1;
            end
        end
        data_size <= data_size + push_cnt - pop_cnt;
    end 
end
// verilator lint_on BLKSEQ

//////////////////////////////////////////////////

int total_cycle_count;

always @(posedge clk or negedge arst_n) begin
    if (~arst_n)
        total_cycle_count <= 0;
    else
        total_cycle_count <= total_cycle_count + 1;
end

generate
    for (genvar gi = 0; gi < M; gi++) begin : gen_write_stats
        int write_cycle_count;

        always @(posedge clk or negedge arst_n) begin
            if (~arst_n)
                write_cycle_count <= 0;
            else if (full_n[gi] & write[gi])
                write_cycle_count <= write_cycle_count + 1;
        end

        final $display("[%m]: %1d/%1d", write_cycle_count, total_cycle_count);
    end
endgenerate

generate
    for (genvar gi = 0; gi < N; gi++) begin : gen_read_stats
        int read_cycle_count;

        always @(posedge clk or negedge arst_n) begin
            if (~arst_n)
                read_cycle_count <= 0;
            else if (empty_n[gi] & read[gi])
                read_cycle_count <= read_cycle_count + 1;
        end

        final $display("[%m]: %1d/%1d", read_cycle_count, total_cycle_count);
    end
endgenerate

//////////////////////////////////////////////////

generate
    for (genvar gi = 1; gi < N; gi++) begin : gen_check_read
        assert property (@(posedge clk) read[gi] -> read[gi-1]) else $fatal(1, "read[x-1] cannot be low when read[x] is high");
    end
endgenerate

endmodule
