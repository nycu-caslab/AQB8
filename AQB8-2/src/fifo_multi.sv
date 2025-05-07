`timescale 1ns / 1ps

module fifo_multi #(
    parameter M          = 2, 
    parameter DATA_WIDTH = 8
)(
    clk, arst_n, full_n, write, din, empty_n, read, dout
);

input                   clk;
input                   arst_n;

output                  full_n  [0:M-1];
input                   write   [0:M-1];
input  [DATA_WIDTH-1:0] din     [0:M-1];

output                  empty_n [0:M-1];
input                   read    [0:M-1];
output [DATA_WIDTH-1:0] dout    [0:M-1];

generate
    for (genvar gi = 0; gi < M; gi++) begin : gen_fifo
        fifo #(
            .DATA_WIDTH(DATA_WIDTH)
        ) fifo_inst (
            .clk(clk),
            .arst_n(arst_n),

            .full_n(full_n[gi]),
            .write(write[gi]),
            .din(din[gi]),

            .empty_n(empty_n[gi]),
            .read(read[gi]),
            .dout(dout[gi])
        );
    end
endgenerate

endmodule
