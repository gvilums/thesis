#include <stdint.h>

// configuration parameters begin

#define INPUT_BUF_SIZE (1 << 16)
#define REDUCTION_VAR_COUNT 16
/*
#define NR_TASKLETS 16
*/

// config params end

typedef uint32_t input_t[2];

typedef uint32_t stage0_out_t;
typedef stage0_out_t stage1_out_t;
typedef uint32_t stage2_out_t;
typedef uint32_t reduction_out_t;

typedef input_t stage0_in_t;
typedef stage0_out_t stage1_in_t;
typedef stage1_out_t stage2_in_t;
typedef stage2_out_t reduction_in_t;

#define GLOBAL_0_OFFSET sizeof(uint32_t)
#define GLOBALS_SIZE (GLOBAL_0_OFFSET + sizeof(stage1_in_t))
#define GLOBALS_SIZE_ALIGNED (((GLOBALS_SIZE - 1) | 7) + 1)
