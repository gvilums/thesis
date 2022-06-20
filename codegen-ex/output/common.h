
#pragma once

#include "stdint.h"
#include "stddef.h"

#define INPUT_BUF_SIZE (1 << 20)

typedef uint32_t elem_count_t;
typedef uint64_t input_t;

typedef uint64_t global_0_t;


typedef input_t stage_0_in_t;
typedef uint64_t stage_0_out_t;

typedef stage_0_out_t output_t;

#define GLOBAL_0_OFFSET sizeof(elem_count_t)
#define GLOBAL_1_OFFSET (GLOBAL_0_OFFSET + sizeof(global_0_t))

#define GLOBALS_SIZE GLOBAL_1_OFFSET
#define GLOBALS_SIZE_ALIGNED (((GLOBALS_SIZE - 1) | 7) + 1)

