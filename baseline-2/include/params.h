#ifndef AQB48_PARAMS_H
#define AQB48_PARAMS_H

#define FIX_CID_TO_ZERO

#define TID_WIDTH           6
#define CID_WIDTH           4
#define RID_WIDTH           (TID_WIDTH + CID_WIDTH)
#define NUM_CONCURRENT_RAYS (1 << CID_WIDTH)

#define NUM_TRIGS_WIDTH     3
#define MAX_TRIGS_PER_NODE  ((1 << NUM_TRIGS_WIDTH) - 1)
#define CHILD_IDX_WIDTH     (32 - NUM_TRIGS_WIDTH)

#define TRIG_SRAM_DEPTH     (MAX_TRIGS_PER_NODE * NUM_CONCURRENT_RAYS)

#define STACK_SIZE_WIDTH    5
#define STACK_SIZE          ((1 << STACK_SIZE_WIDTH) - 1)

#endif  // AQB48_PARAMS_H
