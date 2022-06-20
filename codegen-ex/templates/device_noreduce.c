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
elem_count_t total_input_elems;

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
__mram_noinit uint8_t element_output_buffer[INPUT_BUF_SIZE];
__host elem_count_t total_output_elems;

// element count for each tasklet
uint32_t output_elems[NR_TASKLETS];

// various barriers
BARRIER_INIT(setup_barrier, NR_TASKLETS);
BARRIER_INIT(output_offset_compute, NR_TASKLETS);
BARRIER_INIT(output_finalize, NR_TASKLETS);


uint8_t fixed_output_buffers[NR_TASKLETS][8];


int pipeline(input_t* data_in, output_t* data_out);
void setup_inputs();

void write_output(size_t elem_offset, output_t* data) {
    size_t byte_offset = elem_offset * sizeof(output_t);
    size_t byte_offset_end = byte_offset + sizeof(output_t);

    uint8_t tmp_buf[16 + sizeof(output_t)];

    size_t aligned_offset = byte_offset & ~7;
    size_t aligned_offset_end = ((byte_offset_end - 1) | 7) + 1;


    mram_read(&element_output_buffer[aligned_offset], tmp_buf, aligned_offset_end - aligned_offset);


    memcpy(&tmp_buf[byte_offset - aligned_offset], data, sizeof(output_t));

    mram_write(tmp_buf, &element_output_buffer[aligned_offset], aligned_offset_end - aligned_offset);

    // memcpy(&element_output_buffer[byte_offset], data, sizeof(output_t));
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

    seqreader_buffer_t local_cache = seqread_alloc();
    seqreader_t sr;

    input_t* current_read = seqread_init(
        local_cache, &element_input_buffer[local_offset * sizeof(input_t)], &sr);

    output_elems[index] = 0;
    output_t dummy_output;
    for (size_t i = 0; i < input_elem_count; ++i) {
        output_elems[index] += pipeline(current_read, &dummy_output);
        current_read = seqread_get(current_read, sizeof(input_t), &sr);
    }

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

    size_t current_output_offset = output_offset;
    output_t output_buf;

    size_t i = 0;
    for (; i < input_elem_count; ++i) {
        if (pipeline(current_read, &output_buf)) {
            // TODO this is a direct MRAM access, which is bad. Implement something aking to seqread, but for writing
            write_output(current_output_offset, &output_buf);
            // printf("tasklet %u: %u\n", index, output_buf);

            // fill fixed output buffer
            size_t current_fill = (current_output_offset - output_offset) * sizeof(output_t);
            if (sizeof(output_t) >= 8 - current_fill) {
                memcpy(&fixed_output_buffers[index][current_fill], &output_buf, 8 - current_fill);
                break;
            } else {
                memcpy(&fixed_output_buffers[index][current_fill], &output_buf, sizeof(output_t));
            }

            current_output_offset += 1;
        }
        current_read = seqread_get(current_read, sizeof(input_t), &sr);
    }
    for (; i < input_elem_count; ++i) {
        if (pipeline(current_read, &output_buf)) {
            // TODO this is a direct MRAM access, which is bad. Implement something aking to seqread, but for writing
            write_output(current_output_offset, &output_buf);
            // memcpy(&element_output_buffer[current_output_offset * sizeof(output_t)], &output_buf, sizeof(output_t));
            current_output_offset += 1;
        }
        current_read = seqread_get(current_read, sizeof(input_t), &sr);
    }


    // printf("tasklet %u [%u]: %u %u\n", index, output_elems[index], ((int32_t*)fixed_output_buffers[index])[0], ((int32_t*)fixed_output_buffers[index])[1]);

    barrier_wait(&output_finalize);


    if (index == 0) {
        // {
        //     seqreader_buffer_t local_cache = seqread_alloc();
        //     seqreader_t sr;

        //     output_t* current_read = seqread_init(
        //         local_cache, &element_output_buffer[0], &sr);

        //     for (size_t i = 0; i < total_output_elems; ++i) {
        //         printf("%u ", *current_read);
        //         current_read = seqread_get(current_read, sizeof(output_t), &sr);
        //     }
        //     puts("");
        // }

        size_t offset = 0;
        for (int i = 0; i < NR_TASKLETS - 1; ++i) {
            offset += output_elems[i] * sizeof(output_t);
            // unaligned output
            // printf("offset: %u\n", offset);
            if ((offset & 7) != 0) { 
                // printf("fixing: %u\n", offset);
                // 1. read 8-byte chunk from mram
                // 2. fix it up with the subsequent fixed buffer (maybe use multiple buffers)
                // 3. write back chunk
                uint8_t fixup_buffer[8];
                mram_read(&element_output_buffer[offset & ~7], &fixup_buffer[0], sizeof(fixup_buffer));

                memcpy(&fixup_buffer[offset & 7], fixed_output_buffers[i + 1], 8 - (offset & 7));
                // printf("tasklet %u [%u]: %u %u\n", i + 1, output_elems[i + 1], ((int32_t*)fixed_output_buffers[i + 1])[0], ((int32_t*)fixed_output_buffers[i + 1])[1]);

                mram_write(&fixup_buffer[0], &element_output_buffer[offset & ~7], sizeof(fixup_buffer));
            }
        }
    }
    if (index == 0) {
        puts("ok");
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
    ${ stage["program"] }\
}

    % elif stage["kind"] == "filter":
int stage_${i}(const stage_${i}_in_t* in_ptr) {
    ${ stage["program"] }\
}

    % endif
% endfor

int pipeline(input_t* data_in, output_t* data_out) {
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
        return 0;
    }
    % endif
% endfor

    memcpy(data_out, ${ input_name(stages[-1]) }, sizeof(output_t));

    return 1;
}