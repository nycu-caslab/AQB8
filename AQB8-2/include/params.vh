`ifndef AQB48_PARAMS_H
`define AQB48_PARAMS_H

`define FIX_CID_TO_ZERO

`define TID_WIDTH             6
`define CID_WIDTH             4
`define RID_WIDTH             (`TID_WIDTH + `CID_WIDTH)
`define NUM_CONCURRENT_RAYS   (1 << `CID_WIDTH)

`define FIELD_B_WIDTH         3
`define MAX_TRIGS_PER_NODE    ((1 << `FIELD_B_WIDTH) - 1)
`define FIELD_C_WIDTH         (15 - `FIELD_B_WIDTH)
`define CLUSTER_IDX_WIDTH     (`FIELD_B_WIDTH + `FIELD_C_WIDTH)
`define CHILD_IDX_WIDTH       (`CLUSTER_IDX_WIDTH + `FIELD_C_WIDTH)

`define TRIG_SRAM_DEPTH       (`MAX_TRIGS_PER_NODE * `NUM_CONCURRENT_RAYS)
`define TRIG_SRAM_ADDR_WIDTH  $clog2(`TRIG_SRAM_DEPTH)

`define STACK_SIZE_WIDTH      5
`define STACK_SIZE            ((1 << `STACK_SIZE_WIDTH) - 1)
`define STACK_SRAM_DEPTH      (`STACK_SIZE * `NUM_CONCURRENT_RAYS)
`define STACK_SRAM_ADDR_WIDTH $clog2(`STACK_SRAM_DEPTH)

`define NUM_INIT              1
`define NUM_TRV               1
`define NUM_CLSTR             1
`define NUM_UPDT              1
`define NUM_BBOX              1
`define NUM_IST               1

`endif  // AQB48_PARAMS_H
