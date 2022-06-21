#include "common.h"

#include <barrier.h>
#include <defs.h>
#include <handshake.h>
#include <mram.h>
#include <mutex.h>
#include <seqread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

// host input globals
uint32_t total_input_elems;

% for i, global_value in enumerate(pipeline["globals"]):

global_${i}_t ${global_value["name"]};

% endfor

// constant globals

% for i, constant in enumerate(pipeline["constants"]):

constant_${i}_t ${constant["name"]} = ${constant["init"]};

% endfor

// streaming data input
% for i in range(len(in_stage["inputs"])):
__mram_noinit uint8_t element_input_buffer_${ i }[INPUT_BUF_SIZE];
% endfor
__host uint8_t globals_input_buffer[GLOBALS_SIZE_ALIGNED];

<%block name="global_decl"/>

void setup_inputs() {
    // initialize element count
    memcpy(&total_input_elems, &globals_input_buffer[0], sizeof(total_input_elems));

    // initialize global variables

% for global_value in pipeline["globals"]:
    memcpy(&${ global_value["name"]}, &globals_input_buffer[GLOBAL_${ loop.index }_OFFSET], sizeof(${ global_value["name"] }));
% endfor

}

void pipeline_input(stage_0_out_t* out_ptr\
% for i in range(0, len(in_stage["inputs"])):
, const input_${ i }_t* in_ptr_${ i }\
% endfor
) {
    ${ in_stage["program"] }
}

% for stage in stages:
    % if stage["kind"] == "map":
void stage_${ stage["id"] }(const stage_${ stage["id"] }_in_t* in_ptr, stage_${ stage["id"] }_out_t* out_ptr) {
    ${ stage["program"] }\
}

    % elif stage["kind"] == "filter":
int stage_${ stage["id"] }(const stage_${ stage["id"] }_in_t* in_ptr) {
    ${ stage["program"] }\
}

    % endif
% endfor

<%block name="function_decl"/>

int main() {
    uint32_t index = me();
    if (index == 0) {
        setup_inputs();
    }
    barrier_wait(&setup_barrier);

    size_t input_elem_count = total_input_elems / NR_TASKLETS;
    size_t remaining_elems = total_input_elems % NR_TASKLETS;

    // distribute N remaining elements onto first N tasklets
    if (index < remaining_elems) {
        input_elem_count += 1;
    }

    size_t local_offset = input_elem_count * index;
    if (index >= remaining_elems) {
        local_offset += remaining_elems;
    }

% for i in range(0, len(in_stage["inputs"])):
    seqreader_buffer_t local_cache_${ i } = seqread_alloc();
    seqreader_t sr_${ i };

    input_${ i }_t* current_read_${ i } = seqread_init(
        local_cache_${ i }, &element_input_buffer_${ i }[local_offset * sizeof(input_${ i }_t)], &sr_${ i });
% endfor

<%block name="main_process"/>
    return 0;
}

<%def name="advance_readers()">
% for i in range(0, len(in_stage["inputs"])):
        current_read_${ i } = seqread_get(current_read_${ i }, sizeof(input_${ i }_t), &sr_${ i });
% endfor
</%def>

<%def name="call_with_inputs(name, first)">\
${ name }(${ first }\
% for i in range(0, len(in_stage["inputs"])):
, current_read_${ i }\
% endfor
);\
</%def>