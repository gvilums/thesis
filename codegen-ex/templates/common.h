<%!
    def create_typedef(name: str, type: str) -> str:
        base, bracket, rest = type.partition('[')
        return f"typedef {base} {name}{bracket + rest};"
%>
#pragma once

#include "stdint.h"
#include "stddef.h"

#define REDUCTION_VAR_COUNT ${ pipeline["reduction_vars"] }
#define INPUT_BUF_SIZE (1 << 20)
#define NUM_INPUTS ${ len(in_stage["inputs"]) }

typedef uint32_t elem_count_t;

% for input_type in in_stage["inputs"]:
${ create_typedef(f"input_{ loop.index }_t", input_type) }
% endfor

% for global_value in pipeline["globals"]:
${ create_typedef(f"global_{ loop.index }_t", global_value["type"]) }
% endfor

% for constant in pipeline["constants"]:
${ create_typedef(f"constant_{ loop.index }_t", constant["type"]) }
% endfor

${ create_typedef("stage_0_out_t", in_stage['output']) }

% for stage in stages:
    % if stage["input_idx"] == -1:
${ create_typedef(f"stage_{ stage['id'] }_in_t", "input_t") }
    % else:
${ create_typedef(f"stage_{ stage['id'] }_in_t", f"stage_{stage['input_idx']}_out_t") }
    % endif
    % if "output" in stage:
${ create_typedef(f"stage_{ stage['id'] }_out_t", stage["output"]) }
    % else:
${ create_typedef(f"stage_{ stage['id'] }_out_t", f"stage_{ stage['id'] }_in_t") }
    % endif
% endfor

${ create_typedef("reduction_in_t", f"stage_{reduction['input_idx']}_out_t") }
${ create_typedef("reduction_out_t", reduction["output"]) }
typedef reduction_out_t output_t;

#define GLOBAL_0_OFFSET sizeof(elem_count_t)
% for i in range(1, len(pipeline["globals"]) + 1):
#define GLOBAL_${ i }_OFFSET (GLOBAL_${ i - 1 }_OFFSET + sizeof(global_${ i - 1 }_t))
% endfor

#define GLOBALS_SIZE GLOBAL_${ len(pipeline["globals"]) }_OFFSET
#define GLOBALS_SIZE_ALIGNED (((GLOBALS_SIZE - 1) | 7) + 1)
