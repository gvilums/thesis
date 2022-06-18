
#pragma once

#include "stdint.h"
#include "stddef.h"

#define REDUCTION_VAR_COUNT 12
#define INPUT_BUF_SIZE (1 << 16)
typedef uint32_t input_t[2];
typedef uint32_t elem_count_t;
typedef uint32_t constant_0_t;
typedef input_t stage_0_in_t;
typedef uint32_t stage_0_out_t;
typedef stage_0_out_t stage_1_in_t;
typedef uint32_t stage_1_out_t;
typedef stage_1_in_t reduction_in_t;
typedef stage_1_out_t reduction_out_t;
#define GLOBALS_SIZE sizeof(elem_count_t)
#define GLOBALS_SIZE_ALIGNED (((GLOBALS_SIZE - 1) | 7) + 1)
