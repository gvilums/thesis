
#pragma once

#include "stdint.h"
#include "stddef.h"

#define INPUT_BUF_SIZE (1 << 20)
#define NUM_INPUTS 2

typedef uint32_t elem_count_t;

typedef uint32_t input_0_t;
typedef uint32_t input_1_t;

typedef uint32_t global_0_t;

typedef uint32_t constant_0_t;

typedef uint32_t stage_0_out_t[2];

typedef stage_0_out_t stage_1_in_t;
typedef uint32_t stage_1_out_t;


#define REDUCTION_VAR_COUNT 16
typedef stage_1_out_t reduction_in_t;
typedef uint32_t reduction_out_t;
typedef reduction_out_t output_t;


#define GLOBAL_0_OFFSET sizeof(elem_count_t)
#define GLOBAL_1_OFFSET (GLOBAL_0_OFFSET + sizeof(global_0_t))

#define GLOBALS_SIZE GLOBAL_1_OFFSET
#define GLOBALS_SIZE_ALIGNED (((GLOBALS_SIZE - 1) | 7) + 1)
