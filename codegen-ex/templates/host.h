
#pragma once

#include "common.h"

int process(reduction_out_t* output, const input_t* input, size_t elem_count\
% for global_value in pipeline["globals"]:
, const global_${ loop.index }_t* global_${ loop.index }\
% endfor
);
