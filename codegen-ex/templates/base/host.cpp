#include "common.h"
#include "host.hpp"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
extern "C" {
#include <dpu.h>
}
#include <execution>
#include <algorithm>

#define DPU_BINARY "device"

<%def name="param_decl()">\
% for i in range(0, len(in_stage["inputs"])):
, const input_${ i }_t* input_${ i }\
% endfor
, size_t elem_count\
% for global_value in pipeline["globals"]:
, const global_${ loop.index }_t* global_${ loop.index }\
% endfor
</%def>

<%def name="param_use()">\
% for i in range(0, len(in_stage["inputs"])):
, input_${ i }\
% endfor
    , elem_count\
% for global_value in pipeline["globals"]:
, global_${ loop.index }\
% endfor
</%def>


<%block name="top_level_decl"/>


void setup_inputs(struct dpu_set_t set, uint32_t nr_dpus ${ param_decl() }) {
    struct dpu_set_t dpu;
    uint32_t dpu_id;

    size_t base_inputs = elem_count / nr_dpus;
    size_t remaining_elems = elem_count % nr_dpus;

    size_t dpu_offsets[nr_dpus];
    DPU_FOREACH(set, dpu, dpu_id) {
        size_t input_elem_count = base_inputs;
        if (dpu_id < remaining_elems) {
            input_elem_count += 1;
        }
        size_t local_offset = input_elem_count * dpu_id;
        if (dpu_id >= remaining_elems) {
            local_offset += remaining_elems;
        }
        dpu_offsets[dpu_id] = local_offset;
    }

% for i in range(0, len(in_stage["inputs"])):
    {
        DPU_FOREACH(set, dpu, dpu_id) {
            DPU_ASSERT(dpu_prepare_xfer(dpu, (void*)&input_${ i }[dpu_offsets[dpu_id]]));
        }
        size_t aligned_max_size = ((sizeof(input_${ i }_t) * base_inputs) | 7) + 1;
        DPU_ASSERT(dpu_push_xfer(set, DPU_XFER_TO_DPU, "element_input_buffer_${ i }", 0, aligned_max_size, DPU_XFER_DEFAULT));
    }
%endfor

    uint8_t globals_data_less[GLOBALS_SIZE_ALIGNED];
    memcpy(&globals_data_less[0], &base_inputs, sizeof(elem_count_t));

% for global_value in pipeline["globals"]:
    memcpy(&globals_data_less[GLOBAL_${ loop.index }_OFFSET], global_${ loop.index }, sizeof(global_${ loop.index }_t));
% endfor

    uint8_t globals_data_more[GLOBALS_SIZE_ALIGNED];
    memcpy(globals_data_more, globals_data_less, sizeof(globals_data_more));
    *((uint32_t*)globals_data_more) += 1;

    DPU_FOREACH(set, dpu, dpu_id) {
        if (dpu_id < remaining_elems) {
            DPU_ASSERT(dpu_prepare_xfer(dpu, (void*)globals_data_more));
        } else {
            DPU_ASSERT(dpu_prepare_xfer(dpu, (void*)globals_data_less));
        }
    }
    DPU_ASSERT(dpu_push_xfer(set, DPU_XFER_TO_DPU, "globals_input_buffer", 0, GLOBALS_SIZE_ALIGNED, DPU_XFER_DEFAULT));
}

<%block name="compute_result"/>

<%block name="process"/>

static struct timer global_timer;
static int iter = 0;
static int state = 0;

void timer_start_transfer(void) {
    if (state != 0) {
        puts("invalid timer state on start");
        exit(EXIT_FAILURE);
    }
    ++state;
    clock_gettime(CLOCK_MONOTONIC_RAW, &global_timer.times[iter][0]);
}

void timer_launch_dpus(void) {
    if (state != 1) {
        puts("invalid timer state on launch");
        exit(EXIT_FAILURE);
    }
    ++state;
    clock_gettime(CLOCK_MONOTONIC_RAW, &global_timer.times[iter][1]);
}

void timer_retrieve_data(void) {
    if (state != 2) {
        puts("invalid timer state on retrieve");
        exit(EXIT_FAILURE);
    }
    ++state;
    clock_gettime(CLOCK_MONOTONIC_RAW, &global_timer.times[iter][2]);
}

void timer_start_combine(void) {
    if (state != 3) {
        puts("invalid timer state on combine");
        exit(EXIT_FAILURE);
    }
    ++state;
    clock_gettime(CLOCK_MONOTONIC_RAW, &global_timer.times[iter][3]);
}

void timer_finish(void) {
    if (state != 4) {
        puts("invalid timer state on finish");
        exit(EXIT_FAILURE);
    }
    state = 0;
    clock_gettime(CLOCK_MONOTONIC_RAW, &global_timer.times[iter][4]);
    ++iter;
}

double compute_micro_time_delta(const struct timespec* t0, const struct timespec* t1) {
    long second_delta = t1->tv_sec - t0->tv_sec;
    long nano_delta = t1->tv_nsec - t0->tv_nsec;
    return 1000000 * (double)second_delta + (double)nano_delta / 1000;
}

void timer_print_summary(void) {
    double avg_times[4];
    double total = 0;
    for (int i = 0; i < 4; ++i) {
        double sum = 0;
        for (int j = 0; j < ITERATIONS; ++j) {
            sum += compute_micro_time_delta(&global_timer.times[j][i], &global_timer.times[j][i + 1]);
        }
        sum /= ITERATIONS;

        avg_times[i] = sum;
        total += sum;
    }
    printf("cpu -> dpu transfer %15lf us\n", avg_times[0]);
    printf("dpu execution       %15lf us\n", avg_times[1]);
    printf("dpu -> cpu transfer %15lf us\n", avg_times[2]);
    printf("cpu final combine   %15lf us\n", avg_times[3]);
	printf("-----------------------------------\n");
    printf("total time          %15lf us\n", total);
}
