
#pragma once

#include "stdint.h"
#include "stddef.h"
    #define REDUCTION_VAR_COUNT 12
#define INPUT_BUF_SIZE (1 << 16)
typedef uint8_t input_t[16];
typedef uint32_t elem_count_t;
typedef uint8_t constant_0_t[8];
typedef uint8_t constant_1_t[8];
typedef input_t stage_0_in_t;
typedef uint8_t stage_0_out_t[8];
typedef stage_0_out_t stage_1_in_t;
typedef stage_1_in_t stage_1_out_t;
typedef stage_1_out_t stage_2_in_t;
typedef uint8_t stage_2_out_t[4];
typedef stage_2_in_t reduction_in_t;
typedef stage_2_out_t reduction_out_t;
#define ELEM_COUNT_OFFSET 0
#define DATA_OFFSET (ELEM_COUNT_OFFSET + sizeof(elem_count_t))
