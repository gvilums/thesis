<%!
    def create_typedef(name: str, type: str) -> str:
        base, bracket, rest = type.partition('[')
        return f"typedef {base} {name}{bracket + rest};"
%>
#pragma once

#include "stdint.h"
#include "stddef.h"

#define INPUT_BUF_SIZE (1 << 20)

typedef uint32_t elem_count_t;
${ create_typedef("input_t", pipeline["input"]) }

% for global_value in pipeline["globals"]:
${ create_typedef(f"global_{ loop.index }_t", global_value["type"]) }
% endfor

% for constant in pipeline["constants"]:
${ create_typedef(f"constant_{ loop.index }_t", global_value["type"]) }
% endfor

% for stage in stages:
    % if stage["input_idx"] == -1:
${ create_typedef(f"stage_{ loop.index}_in_t", "input_t") }
    % else:
${ create_typedef(f"stage_{ loop.index }_in_t", f"stage_{stage['input_idx']}_out_t") }
    % endif
    % if "output" in stage:
${ create_typedef(f"stage_{ loop.index }_out_t", stage["output"]) }
    % else:
${ create_typedef(f"stage_{ loop.index }_out_t", f"stage_{ loop.index }_in_t") }
    % endif
% endfor

${ create_typedef("output_t", f"stage_{ len(stages) - 1 }_out_t") }

#define GLOBAL_0_OFFSET sizeof(elem_count_t)
% for global_value in pipeline["globals"][1:]:
#define GLOBAL_${ loop.index + 1}_OFFSET (GLOBAL_${ loop.index }_OFFSET + sizeof(global_${ loop.index }_t))
% endfor
% if len(pipeline["globals"]) > 0:
<%
    idx = len(pipeline["globals"])
%>
#define GLOBAL_${ idx }_OFFSET (GLOBAL_${ idx - 1 }_OFFSET + sizeof(global_${ idx - 1 }_t))
% endif

#define GLOBALS_SIZE GLOBAL_${ len(pipeline["globals"]) }_OFFSET
#define GLOBALS_SIZE_ALIGNED (((GLOBALS_SIZE - 1) | 7) + 1)

