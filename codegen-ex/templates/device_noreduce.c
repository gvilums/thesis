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

// host input globals
elem_count_t total_input_elems;

% for i, global_value in enumerate(pipeline["globals"]):

global_${i}_t ${global_value["name"]};

% endfor

// constant globals

% for i, constant in enumerate(pipeline["constants"]):

constant_${i}_t ${constant["name"]} = ${constant["init"]};

% endfor

// streaming data input
__mram_noinit uint8_t element_input_buffer[INPUT_BUF_SIZE];
__host uint8_t globals_input_buffer[GLOBALS_SIZE_ALIGNED];

// data output
__mram_noinit uint8_t element_output_buffer[INPUT_BUF_SIZE];
__host elem_count_t total_output_elems;

// element count for each tasklet
uint32_t output_elems[NR_TASKLETS];

// various barriers
BARRIER_INIT(setup_barrier, NR_TASKLETS);
BARRIER_INIT(output_offset_compute, NR_TASKLETS);


int pipeline(input_t* data_in, output_t* data_out);
void setup_inputs();

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

    seqreader_buffer_t local_cache = seqread_alloc();
    seqreader_t sr;

    input_t* current_read = seqread_init(
        local_cache, &element_input_buffer[local_offset * sizeof(input_t)], &sr);

    output_elems[index] = 0;
    output_t dummy_output;
    for (size_t i = 0; i < input_elem_count; ++i) {
        output_elems[index] += pipeline(current_read, &dummy_output);
        current_read = seqread_get(current_read, sizeof(input_t), &sr);
    }

    barrier_wait(&output_offset_compute);

    size_t output_offset = 0;
    for (int i = 0; i < index; ++i) {
        output_offset += output_elems[i];
    }

    if (index == NR_TASKLETS - 1) {
        total_output_elems = output_offset + output_elems[index];
    }

    current_read = seqread_init(
        local_cache, &element_input_buffer[local_offset * sizeof(input_t)], &sr);

    size_t current_output_offset = output_offset;
    output_t output_buf;
    for (size_t i = 0; i < input_elem_count; ++i) {
        if (pipeline(current_read, &output_buf)) {
            // printf(" %u: %u %u\n", index, *current_read, output_buf);
            // TODO this is a direct MRAM access, which is bad. Implement something aking to seqread, but for writing
            memcpy(&element_output_buffer[current_output_offset * sizeof(output_t)], &output_buf, sizeof(output_t));
            current_output_offset += 1;
        }
        current_read = seqread_get(current_read, sizeof(input_t), &sr);
    }
    return 0;
}

void setup_inputs() {
    // initialize element count
    memcpy(&total_input_elems, &globals_input_buffer[0], sizeof(total_input_elems));

    // initialize global variables

% for global_value in pipeline["globals"]:
    memcpy(&${ global_value["name"]}, &globals_input_buffer[GLOBAL_${ loop.index }_OFFSET], sizeof(${ global_value["name"] }));
% endfor

}

% for i, stage in enumerate(stages):
    % if stage["kind"] == "map":
void stage_${i}(const stage_${i}_in_t* in_ptr, stage_${i}_out_t* out_ptr) {
    ${ stage["program"] }\
}

    % elif stage["kind"] == "filter":
int stage_${i}(const stage_${i}_in_t* in_ptr) {
    ${ stage["program"] }\
}

    % endif
% endfor

int pipeline(input_t* data_in, output_t* data_out) {
<%!
    def input_name(stage):
        input_idx = stage["input_idx"]
        if input_idx == -1:
            return "data_in"
        else:
            return f"&tmp_{input_idx}"
%>
% for i, stage in enumerate(stages):
    % if stage["kind"] != "filter":
    stage_${ i }_out_t tmp_${ i };
    % endif
% endfor

% for i, stage in enumerate(stages):
    % if stage["kind"] == "map":
    // map
    stage_${i}(${input_name(stage)}, &tmp_${i});
    % elif stage["kind"] == "filter":
    // filter
    if (!stage_${i}(${input_name(stage)})) {
        return 0;
    }
    % endif
% endfor

    memcpy(data_out, ${ input_name(stages[-1]) }, sizeof(output_t));

    return 1;
}