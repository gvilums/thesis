#pragma once

#include "common.h"

#include <time.h>
#include <stdio.h>

<%block name="process_decl"/>

#ifndef ITERATIONS
#define ITERATIONS 20
#endif

#ifndef WARMUP
#define WARMUP 4
#endif

long compute_time_delta(const struct timespec* t0, const struct timespec* t1);

struct timer {
    struct timespec times[ITERATIONS][5];
};

void timer_start_transfer(void);
void timer_launch_dpus(void);
void timer_retrieve_data(void);
void timer_start_combine(void);
void timer_finish(void);

void timer_print_summary(const char* name);