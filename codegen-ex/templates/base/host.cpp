#include "common.h"
#include "host.hpp"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
extern "C" {
#include <dpu.h>
}
// #include <execution>
#include <algorithm>
#include <vector>
#include <thread>
#include <iostream>

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

    elem_count_t elems_per_dpu;
    elem_count_t final_dpu_elems;
    size_t empty_dpus;
    if (elem_count % nr_dpus == 0) {
        elems_per_dpu = elem_count / nr_dpus;
        final_dpu_elems = elems_per_dpu;
        empty_dpus = 0;
    } else {
        elems_per_dpu = elem_count / nr_dpus + 1;
        if (elem_count % elems_per_dpu == 0) {
            final_dpu_elems = elems_per_dpu;
            empty_dpus = nr_dpus - elem_count / elems_per_dpu;
        } else {
            final_dpu_elems = elem_count % elems_per_dpu;
            empty_dpus = nr_dpus - elem_count / elems_per_dpu - 1;
        }
    }

% for i in range(0, len(in_stage["inputs"])):
    {
        DPU_FOREACH(set, dpu, dpu_id) {
            if (dpu_id < nr_dpus - empty_dpus) {
                DPU_ASSERT(dpu_prepare_xfer(dpu, (void*)&input_${ i }[elems_per_dpu * dpu_id]));
            }
        }
        size_t transfer_size = sizeof(input_${ i }_t) * elems_per_dpu;
        DPU_ASSERT(dpu_push_xfer(set, DPU_XFER_TO_DPU, "element_input_buffer_${ i }", 0, align8(transfer_size), DPU_XFER_DEFAULT));
    }
%endfor

    uint8_t globals_data[GLOBALS_SIZE_ALIGNED];
    memcpy(&globals_data[0], &elems_per_dpu, sizeof(elem_count_t));

% for global_value in pipeline["globals"]:
    memcpy(&globals_data[GLOBAL_${ loop.index }_OFFSET], global_${ loop.index }, sizeof(global_${ loop.index }_t));
% endfor

    uint8_t globals_data_end[GLOBALS_SIZE_ALIGNED];
    memcpy(globals_data_end, globals_data, sizeof(globals_data_end));
    memcpy(&globals_data_end[0], &final_dpu_elems, sizeof(elem_count_t));

    uint8_t globals_data_empty[GLOBALS_SIZE_ALIGNED];
    memcpy(globals_data_empty, globals_data, sizeof(globals_data_end));
    elem_count_t no_input = 0;
    memcpy(&globals_data_empty[0], &no_input, sizeof(elem_count_t));

    DPU_FOREACH(set, dpu, dpu_id) {
        if (dpu_id < nr_dpus - empty_dpus - 1) {
            DPU_ASSERT(dpu_prepare_xfer(dpu, (void*)globals_data));
        } else if (dpu_id == nr_dpus - empty_dpus - 1) {
            DPU_ASSERT(dpu_prepare_xfer(dpu, (void*)globals_data_end));
        } else {
            DPU_ASSERT(dpu_prepare_xfer(dpu, (void*)globals_data_empty));
        }
    }
    DPU_ASSERT(dpu_push_xfer(set, DPU_XFER_TO_DPU, "globals_input_buffer", 0, GLOBALS_SIZE_ALIGNED, DPU_XFER_DEFAULT));
    // fprintf(stderr, "transfer globals size: %lu\n", GLOBALS_SIZE_ALIGNED);
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

void timer_print_summary(const char* name) {
    for (int i = 0; i < 4; ++i) {
        double timings[ITERATIONS - WARMUP];
        printf("%s, %d, ", name, i);
        for (int j = 0; j < ITERATIONS - WARMUP; ++j) {
            printf("%lf, ", compute_micro_time_delta(&global_timer.times[WARMUP + j][i], &global_timer.times[WARMUP + j][i + 1]));
        }
        puts("");
    }
    printf("%s, %d, ", name, 4);
    for (int j = 0; j < ITERATIONS - WARMUP; ++j) {
        printf("%lf, ", compute_micro_time_delta(&global_timer.times[WARMUP + j][0], &global_timer.times[WARMUP + j][4]));
    }
    puts("");

    // double avg_times[4];
    // for (int i = 0; i < 4; ++i) {
    //     double sum = 0;
    //     for (int j = 0; j < ITERATIONS; ++j) {
    //         sum += compute_micro_time_delta(&global_timer.times[j][i], &global_timer.times[j][i + 1]);
    //     }
    //     sum /= ITERATIONS;
    //     avg_times[i] = sum;
    // }

    // double total = 0;
    // for (int j = 0; j < ITERATIONS; ++j) {
    //     total += compute_micro_time_delta(&global_timer.times[j][0], &global_timer.times[j][4]);
    // }
    // total /= ITERATIONS;

    // printf("%s\n", name);
    // printf("cpu -> dpu transfer %15lf us\n", avg_times[0]);
    // printf("dpu execution       %15lf us\n", avg_times[1]);
    // printf("dpu -> cpu transfer %15lf us\n", avg_times[2]);
    // printf("cpu final combine   %15lf us\n", avg_times[3]);
	// printf("--------------------------------------\n");
    // printf("total time          %15lf us\n", total);
}
