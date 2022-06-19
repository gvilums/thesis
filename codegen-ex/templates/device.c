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

#if REDUCTION_VAR_COUNT < NR_TASKLETS
#define SYNCHRONIZE_REDUCTION
#elif REDUCTION_VAR_COUNT > NR_TASKLETS
#error Cannot have more reduction variables than tasklets
#endif

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
__mram_noinit uint8_t element_input_buffer[INPUT_BUF_SIZE];
__host uint8_t globals_input_buffer[GLOBALS_SIZE_ALIGNED];

// data output
__host reduction_out_t reduction_output;

// reduction values and helpers
reduction_out_t reduction_vars[REDUCTION_VAR_COUNT];
__atomic_bit uint8_t reduction_mutexes[REDUCTION_VAR_COUNT];

// various barriers
BARRIER_INIT(setup_barrier, NR_TASKLETS);
BARRIER_INIT(reduction_init_barrier, NR_TASKLETS);
BARRIER_INIT(final_reduction_barrier, NR_TASKLETS);


void pipeline(input_t* data_in, uint32_t reduction_idx);
void setup_inputs();
void setup_reduction(uint32_t reduction_idx);
void reduce();

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

    size_t reduction_idx = index % REDUCTION_VAR_COUNT;

    // init local reduction variable
    if (index == reduction_idx) {
        setup_reduction(reduction_idx);
    }
    barrier_wait(&reduction_init_barrier);

    seqreader_buffer_t local_cache = seqread_alloc();
    seqreader_t sr;

    input_t* current_read = seqread_init(
        local_cache, &element_input_buffer[local_offset * sizeof(input_t)], &sr);

    for (size_t i = 0; i < input_elem_count; ++i) {
        pipeline(current_read, reduction_idx);
        current_read = seqread_get(current_read, sizeof(input_t), &sr);
    }

    // only main tasklet performs final reduction
    barrier_wait(&final_reduction_barrier);
    if (index == 0) {
        reduce();
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
    ${ stage["program"] }
}
    % elif stage["kind"] == "filter":
int stage_${i}(const stage_${i}_in_t* in_ptr) {
    ${ stage["program"] }
}
    % endif
% endfor

void pipeline_reduce(reduction_out_t* restrict out_ptr, const reduction_in_t* restrict in_ptr) {
    ${ reduction["program"] }
}

void pipeline_reduce_combine(reduction_out_t* restrict out_ptr, const reduction_out_t* restrict in_ptr) {
    ${ reduction["combine"] }
}

void setup_reduction(uint32_t reduction_idx) {
    memcpy(&reduction_vars[reduction_idx], &${ reduction["identity"] }, sizeof(${ reduction["identity"] }));
}

void pipeline(input_t* data_in, uint32_t reduction_idx) {
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
        return;
    }
    % endif
% endfor

// LOCAL REDUCTION
#ifdef SYNCHRONIZE_REDUCTION
    mutex_lock(&reduction_mutexes[reduction_idx]);
#endif

    pipeline_reduce(&reduction_vars[reduction_idx], ${input_name(reduction)});

#ifdef SYNCHRONIZE_REDUCTION
    mutex_unlock(&reduction_mutexes[reduction_idx]);
#endif
}

void reduce() {
    memcpy(&reduction_output, &reduction_vars[0], sizeof(reduction_vars[0]));
    for (size_t i = 1; i < REDUCTION_VAR_COUNT; ++i) {
        pipeline_reduce_combine(&reduction_output, &reduction_vars[i]);
    }
}
