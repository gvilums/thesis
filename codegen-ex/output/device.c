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

static_assert(sizeof(output_t) % 8 == 0, "output type must be 8-byte aligned");

// host input globals
elem_count_t total_input_elems;


global_0_t scale;


// constant globals


// streaming data input
__mram_noinit uint8_t element_input_buffer[INPUT_BUF_SIZE];
__host uint8_t globals_input_buffer[GLOBALS_SIZE_ALIGNED];

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

// MUTEX_INIT(stdout_mutex);

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

    output_elems[index] = input_elem_count;

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

    init_output_writer(output_offset);
    output_t* current_write = get_output_place();
    for (size_t i = 0; i < input_elem_count; ++i) {
        if (pipeline(current_read, current_write)) {
            confirm_write();
            current_write = get_output_place();
        }
        current_read = seqread_get(current_read, sizeof(input_t), &sr);
    }
    flush_outputs();
    return 0;
}

void setup_inputs() {
    // initialize element count
    memcpy(&total_input_elems, &globals_input_buffer[0], sizeof(total_input_elems));

    // initialize global variables

    memcpy(&scale, &globals_input_buffer[GLOBAL_0_OFFSET], sizeof(scale));

}

void stage_0(const stage_0_in_t* in_ptr, stage_0_out_t* out_ptr) {
    *out_ptr = scale * *in_ptr;
}



int pipeline(input_t* data_in, output_t* data_out) {
    stage_0_out_t tmp_0;

    // map
    stage_0(data_in, &tmp_0);

    memcpy(data_out, &tmp_0, sizeof(output_t));

    return 1;
}


