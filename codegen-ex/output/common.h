
#pragma once

#include "stdint.h"
#include "stddef.h"

#define REDUCTION_VAR_COUNT 16
#define INPUT_BUF_SIZE (1 << 20)

typedef uint8_t input_t;
typedef uint32_t elem_count_t;
typedef uint32_t constant_0_t[256];
typedef input_t stage_0_in_t;
typedef uint32_t stage_0_out_t[256];
typedef stage_0_in_t reduction_in_t;
typedef stage_0_out_t reduction_out_t;

#define GLOBALS_SIZE sizeof(elem_count_t)
#define GLOBALS_SIZE_ALIGNED (((GLOBALS_SIZE - 1) | 7) + 1)

