#include <defs.h>
#include <handshake.h>
#include <mram.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <seqread.h>

// assumptions:
// - LOCAL_BUF_SIZE is always a multiple of INPUT_ELEM_SIZE

#define INPUT_BUF_SIZE (1 << 16)
#define LOCAL_BUF_SIZE (1 << 8)


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


#if INPUT_BUF_SIZE % NR_TASKLETS != 0
#error "Buffer not evenly divided by number of tasklets"
#endif

#if INPUT_BUF_SIZE % LOCAL_BUF_SIZE != 0
#error "Buffer not evenly divided by size of local buffer"
#endif

#if (NR_TASKLETS & (NR_TASKLETS - 1)) != 0
#error "Number of tasklets must be a power of two"
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

// reduction temporaries (one per thread)
uint32_t tl_reduction_val[NR_TASKLETS][REDUCTION_ARR_SIZE];

void pipeline_stage_0_map(const INPUT_TYPE* restrict input,
                          STAGE_0_TYPE* restrict output) {
    // MAP PROGRAM
    for (size_t i = 0; i < STAGE_0_ARR_SIZE; ++i) {
        output[i] = input[i] + input[STAGE_0_ARR_SIZE];
    }
    
}

int pipeline_stage_1_filter(const STAGE_0_TYPE* restrict input) {
    // FILTER PROGRAM
    return 1;
    // return memcmp(input, comparison, STAGE_0_OUT_SIZE) != 0;
}

void pipeline_stage_2_map(const STAGE_1_TYPE* restrict input,
                          STAGE_2_TYPE* restrict output) {
    // MAP PROGRAM
    for (size_t i = 0; i < STAGE_2_ARR_SIZE; ++i) {
        output[i] = input[i];
    }
    // for (size_t i = 0; i < STAGE_2_OUT_SIZE; i += 2) {
        // output[i] = input[i];
        // output[i + 1] = input[i + 1];
        // if (input[i] < input[i + 1]) {
        //     output[i] = input[i];
        //     output[i + 1] = input[i + 1];
        // } else {
        //     output[i] = input[i + 1];
        //     output[i + 1] = input[i];
        // }
    // }
}

void pipeline_reduce(const REDUCTION_TYPE* restrict input0,
                     const REDUCTION_TYPE* restrict input1,
                     REDUCTION_TYPE* restrict output) {
    // REDUCE PROGRAM
    *output = *input0 + *input1;
}

// PROCESSING PIPELINE
void pipeline(uint32_t* data_in, uint32_t index) {
    STAGE_0_TYPE tmp0[STAGE_0_ARR_SIZE];    // stage 0 temporary
    STAGE_2_TYPE tmp1[STAGE_2_ARR_SIZE];    // stage 2 temporary
    REDUCTION_TYPE tmp2[REDUCTION_ARR_SIZE];  // reduction temporary

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

    // printf("%u %u\n", me(), *data_in);

    // LOCAL REDUCTION
    pipeline_reduce(tl_reduction_val[index], tmp1, tmp2);
    memcpy(tl_reduction_val[index], tmp2, sizeof(tmp2));
}

// FINAL REDUCTION
void reduce(uint32_t index) {
    for (size_t i = 2; i <= NR_TASKLETS; i *= 2) {
        if (index % i == 0) {
            size_t partner = index + i / 2;
            handshake_wait_for(partner);

            {
                // PRE-REDUCE
                REDUCTION_TYPE tmp[REDUCTION_ARR_SIZE];
                pipeline_reduce(tl_reduction_val[index],
                                tl_reduction_val[partner], tmp);
                memcpy(tl_reduction_val[index], tmp, sizeof(tmp));
            }
        } else {
            handshake_notify();
            break;
        }
    }
    if (index == 0) {
        memcpy(reduction_output, tl_reduction_val[0], sizeof(tl_reduction_val[0]));
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

    // init local reduction variable
    memcpy(tl_reduction_val[index], zero_vec, sizeof(zero_vec));


    seqreader_buffer_t local_cache = seqread_alloc();
    seqreader_t sr;

    INPUT_TYPE* current_read = seqread_init(local_cache, &input_data_buffer[data_offset * INPUT_ELEM_SIZE], &sr);

    for (size_t i = 0; i < input_elem_count; ++i) {
        pipeline(current_read, index);
        current_read = seqread_get(current_read, INPUT_ELEM_SIZE, &sr);
    }

    // perform final reduction
    reduce(index);

    if (index == 0) {
        printf("successfully executed pipeline\n");
    }
    return 0;
}