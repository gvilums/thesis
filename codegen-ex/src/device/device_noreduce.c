#include <defs.h>
#include <handshake.h>
#include <mram.h>
#include <seqread.h>
#include <mutex.h>
#include <barrier.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// assumptions:

#define INPUT_BUF_SIZE (1 << 16)
#define OUTPUT_BUF_SIZE (1 << 16)

typedef uint32_t _input_t;
typedef uint32_t _stage_0_t;
typedef _stage_0_t _stage_1_t;
typedef uint32_t _stage_2_t;
typedef uint32_t _reduction_t;


#define INPUT_TYPE uint32_t
#define STAGE_0_TYPE uint32_t
#define STAGE_1_TYPE STAGE_0_TYPE
#define STAGE_2_TYPE uint32_t
#define OUTPUT_TYPE STAGE_2_TYPE

#define INPUT_ARR_SIZE 2
#define STAGE_0_ARR_SIZE 1
#define STAGE_1_ARR_SIZE STAGE_0_ARR_SIZE
#define STAGE_2_ARR_SIZE 1
#define OUTPUT_ARR_SIZE STAGE_2_ARR_SIZE

#define INPUT_ELEM_SIZE (sizeof(INPUT_TYPE) * INPUT_ARR_SIZE)
#define OUTPUT_ELEM_SIZE (sizeof(OUTPUT_TYPE) * OUTPUT_ARR_SIZE)

// GLOBAL VARIABLES

// streaming data input
__mram_noinit uint8_t element_input_buffer[INPUT_BUF_SIZE];
__mram_noinit uint8_t output_data_buffer[INPUT_BUF_SIZE];

__host uint32_t total_input_elems;
__host uint32_t total_output_elems;

// global value input
__host STAGE_0_TYPE comparison[STAGE_0_ARR_SIZE];


// values needed for synchronizing reduction
uint32_t elem_counts[NR_TASKLETS];

// barriers
BARRIER_INIT(elem_count_barrier, NR_TASKLETS);


void pipeline_stage_0_map(const INPUT_TYPE* restrict input, STAGE_0_TYPE* restrict output);
int pipeline_stage_1_filter(const STAGE_0_TYPE* restrict input);
void pipeline_stage_2_map(const STAGE_1_TYPE* restrict input, STAGE_2_TYPE* restrict output);

// PROCESSING PIPELINE
int pipeline(INPUT_TYPE* data_in, OUTPUT_TYPE* data_out) {

    STAGE_0_TYPE tmp0[STAGE_0_ARR_SIZE];      // stage 0 temporary
    STAGE_2_TYPE tmp1[STAGE_2_ARR_SIZE];      // stage 2 temporary

    // STAGE 0: MAP
    { pipeline_stage_0_map(data_in, tmp0); }

    // STAGE 1: FILTER
    {
        int output = pipeline_stage_1_filter(tmp0);
        if (!output) {
            return 0;
        }
    }

    // STAGE 2: MAP
    { pipeline_stage_2_map(tmp0, tmp1); }

    return 1;
}

int pipeline_filter_eval(INPUT_TYPE* data_in) {

    STAGE_0_TYPE tmp0[STAGE_0_ARR_SIZE];      // stage 0 temporary
    STAGE_2_TYPE tmp1[STAGE_2_ARR_SIZE];      // stage 2 temporary

    // STAGE 0: MAP
    { pipeline_stage_0_map(data_in, tmp0); }

    // STAGE 1: FILTER
    {
        int output = pipeline_stage_1_filter(tmp0);
        if (!output) {
            return 0;
        }
    }

    return 1;

    // The following stage is not needed, because tailing maps (without filter)
    // cannot influence whether an element is retained in the output

    // STAGE 2: MAP
    // { pipeline_stage_2_map(tmp0, tmp1); }

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

    seqreader_buffer_t local_cache = seqread_alloc();
    seqreader_t sr;

    INPUT_TYPE* current_read =
        seqread_init(local_cache, &element_input_buffer[data_offset * INPUT_ELEM_SIZE], &sr);

    uint32_t elem_count = 0;

    for (size_t i = 0; i < input_elem_count; ++i) {
        elem_count += pipeline_filter_eval(current_read);
        current_read = seqread_get(current_read, INPUT_ELEM_SIZE, &sr);
    }

    barrier_wait(&elem_count_barrier);

    uint32_t output_offset = compute_offset(index);

    current_read = seqread_init(local_cache, &element_input_buffer[data_offset * INPUT_ELEM_SIZE], &sr);
    OUTPUT_TYPE* current_write = seqwrite_init(write_cache, &output_data_buffer[output_offset * OUTPUT_ELEM_SIZE], &sw);

    for (size_t i = 0; i < input_elem_count; ++i) {
        if (pipeline(current_read, current_write)) {
            current_write = seqwrite_get(current_write, OUTPUT_ELEM_SIZE, &sw);
        } 
        current_read = seqread_get(current_read, INPUT_ELEM_SIZE, &sr);
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