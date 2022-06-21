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
% for i in range(len(in_stage["inputs"])):
__mram_noinit uint8_t element_input_buffer_${ i }[INPUT_BUF_SIZE];
% endfor
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


void pipeline(uint32_t reduction_idx\
% for i in range(0, len(in_stage["inputs"])):
    , input_${ i }_t* input_${ i }\
% endfor
);
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

% for i in range(0, len(in_stage["inputs"])):
    seqreader_buffer_t local_cache_${ i } = seqread_alloc();
    seqreader_t sr_${ i };

    input_${ i }_t* current_read_${ i } = seqread_init(
        local_cache_${ i }, &element_input_buffer_${ i }[local_offset * sizeof(input_${ i }_t)], &sr_${ i });
% endfor

    for (size_t i = 0; i < input_elem_count; ++i) {
        pipeline(reduction_idx\
% for i in range(0, len(in_stage["inputs"])):
, current_read_${ i }\
% endfor
);
% for i in range(0, len(in_stage["inputs"])):
        current_read_${ i } = seqread_get(current_read_${ i }, sizeof(input_${ i }_t), &sr_${ i });
% endfor
    }

    // only main tasklet performs final reduction
    barrier_wait(&final_reduction_barrier);
    if (index == 0) {
        reduce();
        puts("dpu ok");
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

void pipeline_reduce(reduction_out_t* restrict out_ptr, const reduction_in_t* restrict in_ptr) {
    ${ reduction["program"] }
}

void pipeline_reduce_combine(reduction_out_t* restrict out_ptr, const reduction_out_t* restrict in_ptr) {
    ${ reduction["combine"] }
}

void setup_reduction(uint32_t reduction_idx) {
    memcpy(&reduction_vars[reduction_idx], &${ reduction["identity"] }, sizeof(${ reduction["identity"] }));
}

void pipeline(uint32_t reduction_idx\
% for i in range(0, len(in_stage["inputs"])):
, input_${ i }_t* input_${ i }\
% endfor
) {
    stage_0_out_t tmp_0;
% for stage in stages:
    % if stage["kind"] != "filter":
    stage_${ stage["id"] }_out_t tmp_${ stage["id"] };
    % endif
% endfor

    pipeline_input(&tmp_0\
% for i in range(0, len(in_stage["inputs"])):
, input_${ i }\
% endfor
);

% for stage in stages:
    % if stage["kind"] == "map":
    // map
    stage_${ stage["id"] }(&tmp_${ stage["input_idx"] }, &tmp_${ stage["id"] });
    % elif stage["kind"] == "filter":
    // filter
    if (!stage_${ stage["id"] }(&tmp_${ stage["input_idx"] })) {
        return;
    }
    % endif
% endfor

// LOCAL REDUCTION
#ifdef SYNCHRONIZE_REDUCTION
    mutex_lock(&reduction_mutexes[reduction_idx]);
#endif

    pipeline_reduce(&reduction_vars[reduction_idx], &tmp_${ reduction["input_idx"] });

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
