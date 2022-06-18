
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

global_0_t global_val;


// constant globals

constant_0_t zero = 0;


// streaming data input
__mram_noinit uint8_t element_input_buffer[INPUT_BUF_SIZE];
__mram_noinit uint8_t globals_input_buffer[GLOBALS_SIZE_ALIGNED];

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

    if (index == 0) {
        printf("ok\n");
    }
    return 0;
}

// GENERATED CODE

void setup_inputs() {
    // read aligned MRAM chunk containing global data
    __dma_aligned uint8_t buf[GLOBALS_SIZE_ALIGNED];
    mram_read(globals_input_buffer, buf, GLOBALS_SIZE_ALIGNED);

    memcpy(&total_input_elems, &buf[0], sizeof(total_input_elems));

    // initialize global variables
    memcpy(&global_val, &buf[GLOBAL_0_OFFSET], sizeof(global_val));

}

void stage_0(const stage_0_in_t* restrict in_ptr, stage_0_out_t* restrict out_ptr) {
    stage_0_in_t in;
    stage_0_out_t out;
    memcpy(&in, in_ptr, sizeof(in));
    {
        // MAP PROGRAM
        out = in[0] + in[1] + global_val;

    }
    memcpy(out_ptr, &out, sizeof(out));
}

void pipeline_reduce(reduction_out_t* restrict out_ptr, const reduction_in_t* restrict in_ptr) {
    reduction_in_t in;
    reduction_out_t out;
    memcpy(&in, in_ptr, sizeof(in));
    memcpy(&out, out_ptr, sizeof(out));
    {
        out += in;

    }
    memcpy(out_ptr, &out, sizeof(out));
}

void pipeline_reduce_combine(reduction_out_t* restrict out_ptr, const reduction_out_t* restrict in_ptr) {
    reduction_out_t in;
    reduction_out_t out;
    memcpy(&in, in_ptr, sizeof(in));
    memcpy(&out, out_ptr, sizeof(out));
    {
        out += in;

    }
    memcpy(out_ptr, &out, sizeof(out));
}

void setup_reduction(uint32_t reduction_idx) {
    memcpy(&reduction_vars[reduction_idx], &zero, sizeof(zero));
}

void pipeline(input_t* data_in, uint32_t reduction_idx) {
    stage_0_in_t tmp_0;
stage_1_in_t tmp_1;


    memcpy(&tmp_0, data_in, sizeof(tmp_0));

    
    stage_0(&tmp_0, &tmp_1);
    

// LOCAL REDUCTION
#ifdef SYNCHRONIZE_REDUCTION
    mutex_lock(&reduction_mutexes[reduction_idx]);
#endif

    pipeline_reduce(&reduction_vars[reduction_idx], &tmp_1);

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
