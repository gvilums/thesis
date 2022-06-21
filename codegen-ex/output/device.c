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
uint32_t total_input_elems;


global_0_t global_val;


// constant globals


constant_0_t zero = 0;


// streaming data input
__mram_noinit uint8_t element_input_buffer_0[INPUT_BUF_SIZE];
__mram_noinit uint8_t element_input_buffer_1[INPUT_BUF_SIZE];
__host uint8_t globals_input_buffer[GLOBALS_SIZE_ALIGNED];


#if REDUCTION_VAR_COUNT < NR_TASKLETS
#define SYNCHRONIZE_REDUCTION
#elif REDUCTION_VAR_COUNT > NR_TASKLETS
#error Cannot have more reduction variables than tasklets
#endif

// data output
__host reduction_out_t reduction_output;

// reduction values and helpers
reduction_out_t reduction_vars[REDUCTION_VAR_COUNT];
__atomic_bit uint8_t reduction_mutexes[REDUCTION_VAR_COUNT];

// various barriers
BARRIER_INIT(setup_barrier, NR_TASKLETS);
BARRIER_INIT(reduction_init_barrier, NR_TASKLETS);
BARRIER_INIT(final_reduction_barrier, NR_TASKLETS);


void setup_inputs() {
    // initialize element count
    memcpy(&total_input_elems, &globals_input_buffer[0], sizeof(total_input_elems));

    // initialize global variables

    memcpy(&global_val, &globals_input_buffer[GLOBAL_0_OFFSET], sizeof(global_val));

}

void pipeline_input(stage_0_out_t* out_ptr, const input_0_t* in_ptr_0, const input_1_t* in_ptr_1) {
    (*out_ptr)[0] = *in_ptr_0;
(*out_ptr)[1] = *in_ptr_1;

}

void stage_1(const stage_1_in_t* in_ptr, stage_1_out_t* out_ptr) {
    *out_ptr = (*in_ptr)[0] + (*in_ptr)[1] + global_val;
}



void pipeline_reduce(reduction_out_t* restrict out_ptr, const reduction_in_t* restrict in_ptr) {
    *out_ptr += *in_ptr;

}

void pipeline_reduce_combine(reduction_out_t* restrict out_ptr, const reduction_out_t* restrict in_ptr) {
    *out_ptr += *in_ptr;

}

void reduce() {
    memcpy(&reduction_output, &reduction_vars[0], sizeof(reduction_vars[0]));
    for (size_t i = 1; i < REDUCTION_VAR_COUNT; ++i) {
        pipeline_reduce_combine(&reduction_output, &reduction_vars[i]);
    }
}

void pipeline(uint32_t reduction_idx, input_0_t* input_0, input_1_t* input_1) {
    stage_0_out_t tmp_0;
    stage_1_out_t tmp_1;

    pipeline_input(&tmp_0, input_0, input_1);

    // map
    stage_1(&tmp_0, &tmp_1);

// LOCAL REDUCTION
#ifdef SYNCHRONIZE_REDUCTION
    mutex_lock(&reduction_mutexes[reduction_idx]);
#endif

    pipeline_reduce(&reduction_vars[reduction_idx], &tmp_1);

#ifdef SYNCHRONIZE_REDUCTION
    mutex_unlock(&reduction_mutexes[reduction_idx]);
#endif
}




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

    seqreader_buffer_t local_cache_0 = seqread_alloc();
    seqreader_t sr_0;

    input_0_t* current_read_0 = seqread_init(
        local_cache_0, &element_input_buffer_0[local_offset * sizeof(input_0_t)], &sr_0);
    seqreader_buffer_t local_cache_1 = seqread_alloc();
    seqreader_t sr_1;

    input_1_t* current_read_1 = seqread_init(
        local_cache_1, &element_input_buffer_1[local_offset * sizeof(input_1_t)], &sr_1);


    // init local reduction variable
    if (index == reduction_idx) {
        memcpy(&reduction_vars[reduction_idx], &zero, sizeof(zero));
    }
    barrier_wait(&reduction_init_barrier);

    for (size_t i = 0; i < input_elem_count; ++i) {
        pipeline(reduction_idx, current_read_0, current_read_1);
        current_read_0 = seqread_get(current_read_0, sizeof(input_0_t), &sr_0);
        current_read_1 = seqread_get(current_read_1, sizeof(input_1_t), &sr_1);
    }

    // only main tasklet performs final reduction
    barrier_wait(&final_reduction_barrier);
    if (index == 0) {
        reduce();
        puts("dpu ok");
    }

    return 0;
}
