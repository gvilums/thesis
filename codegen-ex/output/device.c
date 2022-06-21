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


// constant globals


// streaming data input
__mram_noinit uint8_t element_input_buffer_0[INPUT_BUF_SIZE];
__mram_noinit uint8_t element_input_buffer_1[INPUT_BUF_SIZE];
__host uint8_t globals_input_buffer[GLOBALS_SIZE_ALIGNED];


static_assert(sizeof(output_t) % 8 == 0, "output type must be 8-byte aligned");
// data output
__mram_noinit uint8_t element_output_buffer[INPUT_BUF_SIZE];
__host elem_count_t total_output_elems;


// various barriers
BARRIER_INIT(setup_barrier, NR_TASKLETS);
BARRIER_INIT(output_offset_compute, NR_TASKLETS);

// output handling
#define MRAM_ALIGN 8
#define LOCAL_OUTBUF_SIZE (1 << 8)

// element count for each tasklet
uint32_t output_elems[NR_TASKLETS];

__dma_aligned uint8_t local_output_buffers[NR_TASKLETS][LOCAL_OUTBUF_SIZE];
uint32_t local_outbuf_sizes[NR_TASKLETS];
uint32_t current_output_offsets[NR_TASKLETS];


void setup_inputs() {
    // initialize element count
    memcpy(&total_input_elems, &globals_input_buffer[0], sizeof(total_input_elems));

    // initialize global variables


}

void pipeline_input(stage_0_out_t* out_ptr, const input_0_t* in_ptr_0, const input_1_t* in_ptr_1) {
    (*out_ptr)[0] = *in_ptr_0;
(*out_ptr)[1] = *in_ptr_1;

}

int stage_1(const stage_1_in_t* in_ptr) {
    return 1;
}

void stage_2(const stage_2_in_t* in_ptr, stage_2_out_t* out_ptr) {
    *out_ptr = (*in_ptr)[0] + (*in_ptr)[1];
}



void flush_outputs() {
    uint32_t i = me();

    // copy data from local output buffer into correct location in global buffer
    // mutex_lock(stdout_mutex);
    // for (int j = 0; j < local_outbuf_sizes[i] / sizeof(output_t); ++j) {
    //     printf("%x ", local_output_buffers[i][j * sizeof(output_t)]);
    // }
    // puts("");
    // mutex_unlock(stdout_mutex);
    mram_write(&local_output_buffers[i][0], &element_output_buffer[current_output_offsets[i]], local_outbuf_sizes[i]);
    // advance offset by size of data we just copied
    current_output_offsets[i] += local_outbuf_sizes[i];
    // reset the local output buffer to size 0
    local_outbuf_sizes[i] = 0;
}

output_t* get_output_place() {
    uint32_t i = me();

    // if adding another value of type output_t would overflow the buffer, flush
    if (local_outbuf_sizes[i] + sizeof(output_t) > LOCAL_OUTBUF_SIZE) {
        flush_outputs();
    }

    output_t* place = (output_t*)&local_output_buffers[i][local_outbuf_sizes[i]];
    return place;
}

void confirm_write() {
    local_outbuf_sizes[me()] += sizeof(output_t);
}

void init_output_writer(size_t element_offset) {
    uint32_t i = me();

    local_outbuf_sizes[i] = 0;
    current_output_offsets[i] = element_offset * sizeof(output_t);
}

int pipeline(output_t* data_out, input_0_t* input_0, input_1_t* input_1) {
    stage_0_out_t tmp_0;
    stage_2_out_t tmp_2;

    pipeline_input(&tmp_0, input_0, input_1);

    // filter
    if (!stage_1(&tmp_0)) {
        return 0;
    }
    // map
    stage_2(&tmp_0, &tmp_2);

    memcpy(data_out, &tmp_2, sizeof(output_t));
    return 1;
}


int pipeline_eval_condition(size_t dummy, input_0_t* input_0, input_1_t* input_1) {
    stage_0_out_t tmp_0;
    stage_2_out_t tmp_2;

    pipeline_input(&tmp_0, input_0, input_1);

    // filter
    if (!stage_1(&tmp_0)) {
        return 0;
    }

    return 1;
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

    seqreader_buffer_t local_cache_0 = seqread_alloc();
    seqreader_t sr_0;

    input_0_t* current_read_0 = seqread_init(
        local_cache_0, &element_input_buffer_0[local_offset * sizeof(input_0_t)], &sr_0);
    seqreader_buffer_t local_cache_1 = seqread_alloc();
    seqreader_t sr_1;

    input_1_t* current_read_1 = seqread_init(
        local_cache_1, &element_input_buffer_1[local_offset * sizeof(input_1_t)], &sr_1);


    output_elems[index] = 0;
    for (size_t i = 0; i < input_elem_count; ++i) {
        output_elems[index] += pipeline_eval_condition(0, current_read_0, current_read_1);
        
        current_read_0 = seqread_get(current_read_0, sizeof(input_0_t), &sr_0);
        current_read_1 = seqread_get(current_read_1, sizeof(input_1_t), &sr_1);

    }

    barrier_wait(&output_offset_compute);

    size_t output_offset = 0;
    for (int i = 0; i < index; ++i) {
        output_offset += output_elems[i];
    }

    if (index == NR_TASKLETS - 1) {
        total_output_elems = output_offset + output_elems[index];
    }

    // reset readers
    current_read_0 = seqread_init(
        local_cache_0, &element_input_buffer_0[local_offset * sizeof(input_0_t)], &sr_0);
    current_read_1 = seqread_init(
        local_cache_1, &element_input_buffer_1[local_offset * sizeof(input_1_t)], &sr_1);

    init_output_writer(output_offset);
    output_t* current_write = get_output_place();
    for (size_t i = 0; i < input_elem_count; ++i) {
        int result = pipeline(current_write, current_read_0, current_read_1);
        
        current_read_0 = seqread_get(current_read_0, sizeof(input_0_t), &sr_0);
        current_read_1 = seqread_get(current_read_1, sizeof(input_1_t), &sr_1);

        if (result) {
            confirm_write();
            current_write = get_output_place();
        }
    }
    flush_outputs();

    return 0;
}



