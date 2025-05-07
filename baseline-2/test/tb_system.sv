`timescale 1ns / 1ps
`include "../include/datatypes.svh"

`define CYCLE_TIME 2.0

logic [`NBP_WIDTH-1:0] nbp_ [$];
logic [`TRIG_WIDTH-1:0] trig_ [$];

`include "sim_bbox_mem.sv"
`include "sim_ist_mem.sv"
`include "sim_rtcore.sv"

module tb_system;

// input
logic                  clk;
logic                  arst_n;
logic                  ray_stream_write;
logic [`RAY_WIDTH-1:0] ray_stream_din;
logic                  result_stream_read;

// output
logic                     ray_stream_full_n;
logic                     result_stream_empty_n;
logic [`RESULT_WIDTH-1:0] result_stream_dout;

sim_rtcore sim_rtcore_inst (
    .clk(clk),
    .arst_n(arst_n),

    .ray_stream_full_n(ray_stream_full_n),
    .ray_stream_write(ray_stream_write),
    .ray_stream_din(ray_stream_din),

    .result_stream_empty_n(result_stream_empty_n),
    .result_stream_read(result_stream_read),
    .result_stream_dout(result_stream_dout)
);

//////////////////////////////////////////////////

initial begin
    clk                = 0;
    ray_stream_write   = 0;
    ray_stream_din     = {`RAY_WIDTH{1'bx}};
    result_stream_read = 0;

    force clk = 0;
    arst_n = 1;
    #(100.0);
    arst_n = 0;
    #(100.0);
    arst_n = 1;
    #(100.0);
    release clk;
end

initial begin
    int fd;

    fd = $fopen("../data/gen_nbp.bin", "rb");
    assert(fd != 0);

    forever begin
        reg [         511:0] gen_nbp;
        reg [`NBP_WIDTH-1:0] nbp;

        if ($fread(gen_nbp, fd) == 0)
            break;
        gen_nbp = {<<byte{gen_nbp}};
        nbp = {gen_nbp[32*4+:32*12], 
               gen_nbp[32*3+:`CHILD_IDX_WIDTH], gen_nbp[32*2+:`NUM_TRIGS_WIDTH],
               gen_nbp[32*1+:`CHILD_IDX_WIDTH], gen_nbp[32*0+:`NUM_TRIGS_WIDTH]};
        
        nbp_.push_back(nbp);
    end

    $fclose(fd);
end

initial begin
    int fd;

    fd = $fopen("../data/trig.bin", "rb");
    assert(fd != 0);

    forever begin
        reg [`TRIG_WIDTH-1:0] trig;

        if ($fread(trig, fd) == 0)
            break;
        trig = {<<byte{trig}};
        
        trig_.push_back(trig);
    end

    $fclose(fd);
end

// verilator lint_off BLKSEQ
always #(`CYCLE_TIME/2.0) clk = ~clk;
// verilator lint_on BLKSEQ

//////////////////////////////////////////////////

// verilator lint_off INITIALDLY
initial begin
    int fd;

    @(posedge clk);

    fd = $fopen("../data/ray.bin", "rb");
    assert(fd != 0);

    forever begin
        reg [`RAY_WIDTH-1:0] ray;

        if ($fread(ray, fd) == 0)
            break;
        ray = {<<byte{ray}};

        ray_stream_write <= 1;
        ray_stream_din   <= ray;

        forever @(posedge clk)
            if (ray_stream_full_n)
                break;

        ray_stream_write <= 0;
        ray_stream_din   <= {`RAY_WIDTH{1'bx}};
    end

    $fclose(fd);
end
// verilator lint_on INITIALDLY

// verilator lint_off INITIALDLY
initial begin
    int fd;
    int num_correct;

    @(posedge clk);

    fd = $fopen("../data/result.bin", "rb");
    assert(fd != 0);

    num_correct = 0;
    for (int i = 0; ; i++) begin
        bit [            127:0] result_ref;
        bit [`RESULT_WIDTH-1:0] result;
        bit                     correct;

        if ($fread(result_ref, fd) == 0)
            break;
        result_ref = {<<byte{result_ref}};

        result_stream_read <= 1;

        forever @(posedge clk)
            if (result_stream_empty_n)
                break;
        result = result_stream_dout;

        correct = 0;
        if (result_ref[0] === 1) begin
            if (result[0] === 1 &&
                result[1+32*0+:32] === result_ref[32*1+:32] &&
                result[1+32*1+:32] === result_ref[32*2+:32] &&
                result[1+32*2+:32] === result_ref[32*3+:32])
                correct = 1;
        end else if (result_ref[0] === 0) begin
            if (result[0] === 0)
                correct = 1;
        end else begin
            $fatal(1, "FATAL: INVALID result_ref");
        end

        if (correct) begin
            num_correct++;
        end else begin
            $display("differ in %d", i);
            $display("  %b (golden = %b)", result[         0], result_ref[       0]);
            $display("  %h (golden = %h)", result[1+32*0+:32], result_ref[32*1+:32]);
            $display("  %h (golden = %h)", result[1+32*1+:32], result_ref[32*2+:32]);
            $display("  %h (golden = %h)", result[1+32*2+:32], result_ref[32*3+:32]);
        end

        if (i % 1000 == 0)
            $display("%d @ %t", i, $time);
    end

    $fclose(fd);

    $display("num_correct: %d", num_correct);
    $finish;
end
// verilator lint_on INITIALDLY

endmodule
