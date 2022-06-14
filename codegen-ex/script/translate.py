template = """
#include <defs.h>
#include <handshake.h>
#include <mram.h>
#include <seqread.h>
#include <mutex.h>
#include <barrier.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>


#define INPUT_BUF_SIZE (1 << 16)
#define REDUCTION_VAR_COUNT {red_val_cnt}


typedef uint32_t _input_t;
typedef uint32_t _stage_0_t;
typedef _stage_0_t _stage_1_t;
typedef uint32_t _stage_2_t;
typedef uint32_t _reduction_t;


#define INPUT_TYPE uint32_t
#define STAGE_0_TYPE uint32_t
#define STAGE_1_TYPE STAGE_0_TYPE
#define STAGE_2_TYPE uint32_t
#define REDUCTION_TYPE uint32_t

#define INPUT_ARR_SIZE 2
#define STAGE_0_ARR_SIZE 1
#define STAGE_1_ARR_SIZE STAGE_0_ARR_SIZE
#define STAGE_2_ARR_SIZE 1
#define REDUCTION_ARR_SIZE 1

#define INPUT_ELEM_SIZE (sizeof(INPUT_TYPE) * INPUT_ARR_SIZE)
#define REDUCTION_ELEM_SIZE (sizeof(REDUCTION_TYPE) * REDUCTION_ARR_SIZE)

#if REDUCTION_VAR_COUNT < NR_TASKLETS
#define SYNCHRONIZE_REDUCTION
#elif REDUCTION_VAR_COUNT > NR_TASKLETS
#error Cannot have more reduction variables than tasklets
#endif

// GLOBAL VARIABLES

// streaming data input
__mram_noinit uint8_t input_data_buffer[INPUT_BUF_SIZE];
__host uint32_t total_input_elems;

// streaming data output
__host REDUCTION_TYPE reduction_output[REDUCTION_ARR_SIZE];

// global value input
__host STAGE_0_TYPE comparison[STAGE_0_ARR_SIZE];
__host REDUCTION_TYPE zero_vec[REDUCTION_ARR_SIZE];


// values needed for synchronizing reduction
REDUCTION_TYPE reduction_vars[REDUCTION_VAR_COUNT][REDUCTION_ARR_SIZE];
uint8_t __attribute__((section(".atomic"))) reduction_mutexes[REDUCTION_VAR_COUNT];
BARRIER_INIT(final_reduction_barrier, NR_TASKLETS);
BARRIER_INIT(reduction_init_barrier, NR_TASKLETS);


void pipeline_stage_0_map(const INPUT_TYPE* restrict input, STAGE_0_TYPE* restrict output);
int pipeline_stage_1_filter(const STAGE_0_TYPE* restrict input);
void pipeline_stage_2_map(const STAGE_1_TYPE* restrict input, STAGE_2_TYPE* restrict output);
void pipeline_reduce(REDUCTION_TYPE* restrict accumulator, const STAGE_2_TYPE* restrict input);
void pipeline_reduce_combine(REDUCTION_TYPE* restrict target, const REDUCTION_TYPE* restrict input);

// PROCESSING PIPELINE
void pipeline(uint32_t* data_in, uint32_t reduction_idx) {

    STAGE_0_TYPE tmp0[STAGE_0_ARR_SIZE];      // stage 0 temporary
    STAGE_2_TYPE tmp1[STAGE_2_ARR_SIZE];      // stage 2 temporary

    // STAGE 0: MAP
    { pipeline_stage_0_map(data_in, tmp0); }

    // STAGE 1: FILTER
    {
        int output = pipeline_stage_1_filter(tmp0);
        if (!output) {
            return;
        }
    }

    // STAGE 2: MAP
    { pipeline_stage_2_map(tmp0, tmp1); }

    // LOCAL REDUCTION
    #ifdef SYNCHRONIZE_REDUCTION
    mutex_lock(&reduction_mutexes[reduction_idx]);
    #endif

    pipeline_reduce(reduction_vars[reduction_idx], tmp1);
    #ifdef SYNCHRONIZE_REDUCTION
    mutex_unlock(&reduction_mutexes[reduction_idx]);
    #endif

}

// FINAL REDUCTION
void reduce_(uint32_t index) {
    for (size_t i = 2; i <= NR_TASKLETS; i *= 2) {
        if (index % i == 0) {
            size_t partner = index + i / 2;
            handshake_wait_for(partner);

            {
                // PRE-REDUCE
                REDUCTION_TYPE tmp[REDUCTION_ARR_SIZE];
                pipeline_reduce_combine(reduction_vars[index], reduction_vars[partner]);
            }
        } else {
            handshake_notify();
            break;
        }
    }
    if (index == 0) {
        memcpy(reduction_output, reduction_vars[0], sizeof(reduction_vars[0]));
    }
}

void reduce() {
    memcpy(reduction_output, reduction_vars[0], sizeof(reduction_vars[0]));
    for (size_t i = 1; i < REDUCTION_VAR_COUNT; ++i) {
        pipeline_reduce_combine(reduction_output, reduction_vars[i]);
    }
}

int main() {
    uint32_t index = me();

    size_t input_elem_count = total_input_elems / NR_TASKLETS;
    size_t remaining_elems = total_input_elems % NR_TASKLETS;

    // distribute N remaining elements onto first N tasklets
    if (index < remaining_elems) {
        input_elem_count += 1;
    }

    size_t data_offset = input_elem_count * index;
    if (index >= remaining_elems) {
        data_offset += remaining_elems;
    }

    size_t reduction_idx = index % REDUCTION_VAR_COUNT;

    // init local reduction variable
    if (index == reduction_idx) {
        memcpy(reduction_vars[reduction_idx], zero_vec, sizeof(zero_vec));
    }
    barrier_wait(&reduction_init_barrier);

    seqreader_buffer_t local_cache = seqread_alloc();
    seqreader_t sr;

    INPUT_TYPE* current_read =
        seqread_init(local_cache, &input_data_buffer[data_offset * INPUT_ELEM_SIZE], &sr);

    for (size_t i = 0; i < input_elem_count; ++i) {
        pipeline(current_read, reduction_idx);
        current_read = seqread_get(current_read, INPUT_ELEM_SIZE, &sr);
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

#undef IN_SIZE
#undef OUT_SIZE
#define IN_SIZE INPUT_ARR_SIZE
#define OUT_SIZE STAGE_0_ARR_SIZE

void pipeline_stage_0_map(const INPUT_TYPE* restrict input, STAGE_0_TYPE* restrict output) {
    // MAP PROGRAM
    for (size_t i = 0; i < STAGE_0_ARR_SIZE; ++i) {
        output[i] = input[i] + input[STAGE_0_ARR_SIZE];
    }
}

#undef IN_SIZE
#define IN_SIZE STAGE_0_ARR_SIZE

int pipeline_stage_1_filter(const STAGE_0_TYPE* restrict input) {
    // FILTER PROGRAM
    return 1;
    // return memcmp(input, comparison, STAGE_0_OUT_SIZE) != 0;
}

#undef IN_SIZE
#undef OUT_SIZE
#define IN_SIZE STAGE_1_ARR_SIZE
#define OUT_SIZE STAGE_2_ARR_SIZE

void pipeline_stage_2_map(const STAGE_1_TYPE* restrict input, STAGE_2_TYPE* restrict output) {
    // MAP PROGRAM
    for (size_t i = 0; i < STAGE_2_ARR_SIZE; ++i) {
        output[i] = input[i];
    }
}

#undef IN_SIZE
#undef OUT_SIZE
#undef ACCUM_SIZE
#define IN_SIZE INPUT_ARR_SIZE
#define OUT_SIZE STAGE_0_ARR_SIZE
#define ACCUM_SIZE STAGE_0_ARR_SIZE

void pipeline_reduce(REDUCTION_TYPE* restrict accumulator, const STAGE_2_TYPE* restrict input) {
    *accumulator += *input;
}


void pipeline_reduce_combine(REDUCTION_TYPE* restrict target, const REDUCTION_TYPE* restrict input) {
    *target += *input;
}
"""

print(template.format(red_val_cnt=10))