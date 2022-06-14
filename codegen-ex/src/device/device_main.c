#include "setup.h"

#include <barrier.h>
#include <defs.h>
#include <handshake.h>
#include <mram.h>
#include <mutex.h>
#include <seqread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// configuration parameters begin

#define INPUT_BUF_SIZE (1 << 16)
#define REDUCTION_VAR_COUNT 16
/*
#define NR_TASKLETS 16
*/

typedef uint32_t input_t[2];

typedef uint32_t stage0_out_t;
typedef stage0_out_t stage1_out_t;
typedef uint32_t stage2_out_t;
typedef uint32_t reduction_out_t;

typedef input_t stage0_in_t;
typedef stage0_out_t stage1_in_t;
typedef stage1_out_t stage2_in_t;
typedef stage2_out_t reduction_in_t;

// config params end

#if REDUCTION_VAR_COUNT < NR_TASKLETS
#define SYNCHRONIZE_REDUCTION
#elif REDUCTION_VAR_COUNT > NR_TASKLETS
#error Cannot have more reduction variables than tasklets
#endif

#define ELEM_COUNT_OFFSET 0
#define GLOBAL_0_OFFSET (ELEM_COUNT_OFFSET + sizeof(uint32_t))
#define DATA_OFFSET (GLOBAL_0_OFFSET + sizeof(stage1_in_t))

// host input globals
uint32_t total_input_elems;

stage1_in_t comparison;

// constant globals
reduction_out_t zero_vec = 0;

// streaming data input
__mram_noinit uint8_t input_data_buffer[INPUT_BUF_SIZE];

// data output
__host reduction_out_t reduction_output;

// redution values and helpers
reduction_out_t reduction_vars[REDUCTION_VAR_COUNT];

uint8_t __attribute__((section(".atomic"))) reduction_mutexes[REDUCTION_VAR_COUNT];
BARRIER_INIT(setup_barrier, NR_TASKLETS);
BARRIER_INIT(reduction_init_barrier, NR_TASKLETS);
BARRIER_INIT(final_reduction_barrier, NR_TASKLETS);

void pipeline_stage_0_map(const stage0_in_t*, stage0_out_t*);
int pipeline_stage_1_filter(const stage1_in_t*);
void pipeline_stage_2_map(const stage2_in_t*, stage2_out_t*);
void pipeline_reduce(reduction_out_t*, const reduction_in_t*);
void pipeline_reduce_combine(reduction_out_t*, const reduction_out_t*);

// PROCESSING PIPELINE
void pipeline(input_t* data_in, uint32_t reduction_idx) {
    stage0_out_t tmp0;
    stage2_out_t tmp1;

    // STAGE 0: MAP
    { pipeline_stage_0_map(data_in, &tmp0); }

    // STAGE 1: FILTER
    {
        int output = pipeline_stage_1_filter(&tmp0);
        if (!output) {
            return;
        }
    }

    // STAGE 2: MAP
    { pipeline_stage_2_map(&tmp0, &tmp1); }

// LOCAL REDUCTION
#ifdef SYNCHRONIZE_REDUCTION
    mutex_lock(&reduction_mutexes[reduction_idx]);
#endif

    pipeline_reduce(&reduction_vars[reduction_idx], &tmp1);
#ifdef SYNCHRONIZE_REDUCTION
    mutex_unlock(&reduction_mutexes[reduction_idx]);
#endif
}

// FINAL REDUCTION
// void reduce_(uint32_t index) {
//     for (size_t i = 2; i <= NR_TASKLETS; i *= 2) {
//         if (index % i == 0) {
//             size_t partner = index + i / 2;
//             handshake_wait_for(partner);

//             {
//                 // PRE-REDUCE
//                 REDUCTION_TYPE tmp[REDUCTION_ARR_SIZE];
//                 pipeline_reduce_combine(reduction_vars[index], reduction_vars[partner]);
//             }
//         } else {
//             handshake_notify();
//             break;
//         }
//     }
//     if (index == 0) {
//         memcpy(reduction_output, reduction_vars[0], sizeof(reduction_vars[0]));
//     }
// }

void setup_inputs() {
    // read aligned MRAM chunk containing global data
    __dma_aligned uint8_t buf[DATA_OFFSET + 8];
    mram_read(input_data_buffer, buf, ((DATA_OFFSET - 1) | 7) + 1);

    // initialize global variables
    memcpy(&total_input_elems, &buf[ELEM_COUNT_OFFSET], sizeof(total_input_elems));
    memcpy(&comparison, &buf[GLOBAL_0_OFFSET], sizeof(comparison));
}

void reduce() {
    memcpy(&reduction_output, &reduction_vars[0], sizeof(reduction_vars[0]));
    for (size_t i = 1; i < REDUCTION_VAR_COUNT; ++i) {
        pipeline_reduce_combine(&reduction_output, &reduction_vars[i]);
    }
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

    // init local reduction variable
    if (index == reduction_idx) {
        memcpy(&reduction_vars[reduction_idx], &zero_vec, sizeof(zero_vec));
    }
    barrier_wait(&reduction_init_barrier);

    seqreader_buffer_t local_cache = seqread_alloc();
    seqreader_t sr;

    input_t* current_read = seqread_init(
        local_cache, &input_data_buffer[DATA_OFFSET + local_offset * sizeof(input_t)], &sr);

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

void pipeline_stage_0_map(const stage0_in_t* restrict in_ptr, stage0_out_t* restrict out_ptr) {
    stage0_in_t in;
    stage0_out_t out;
    memcpy(&in, in_ptr, sizeof(in));
    {
        // MAP PROGRAM
        out = in[0] + in[1];
    }
    memcpy(out_ptr, &out, sizeof(out));
}

int pipeline_stage_1_filter(const stage1_in_t* restrict in_ptr) {
    stage1_in_t in;
    memcpy(&in, in_ptr, sizeof(in));
    {
        // FILTER PROGRAM
        return 1;
        // return memcmp(input, comparison, STAGE_0_OUT_SIZE) != 0;
    }
}

void pipeline_stage_2_map(const stage2_in_t* restrict in_ptr, stage2_out_t* restrict out_ptr) {
    stage2_in_t in;
    stage2_out_t out;
    memcpy(&in, in_ptr, sizeof(in));
    {
        // MAP PROGRAM
        out = in;
    }
    memcpy(out_ptr, &out, sizeof(out));
}

void pipeline_reduce(reduction_out_t* restrict out_ptr, const reduction_in_t* restrict in_ptr) {
    reduction_in_t in;
    reduction_out_t out;
    memcpy(&in, in_ptr, sizeof(in));
    memcpy(&out, out_ptr, sizeof(out));
    { out += in; }
    memcpy(out_ptr, &out, sizeof(out));
}

void pipeline_reduce_combine(reduction_out_t* restrict out_ptr,
                             const reduction_out_t* restrict in_ptr) {
    reduction_out_t in;
    reduction_out_t out;
    memcpy(&in, in_ptr, sizeof(in));
    memcpy(&out, out_ptr, sizeof(out));
    { out += in; }
    memcpy(out_ptr, &out, sizeof(out));
}