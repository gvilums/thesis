
#pragma once

#include "stdint.h"
#include "stddef.h"

#define INPUT_BUF_SIZE (1 << 20)
#define NUM_INPUTS 2

#define NR_REDUCTION_VARS 11
#define READ_CACHE_SIZE 1024
#define WRITE_CACHE_SIZE 1024

typedef uint32_t elem_count_t;

typedef uint64_t input_0_t;
typedef uint64_t input_1_t;



typedef uint64_t stage_0_out_t[2];

typedef stage_0_out_t stage_1_in_t;
typedef uint64_t stage_1_out_t;


typedef stage_1_out_t output_t;


#define GLOBAL_0_OFFSET sizeof(elem_count_t)

#define GLOBALS_SIZE GLOBAL_0_OFFSET
#define GLOBALS_SIZE_ALIGNED (((GLOBALS_SIZE - 1) | 7) + 1)


// #define SEQREAD_CACHE_SIZE READ_CACHE_SIZE