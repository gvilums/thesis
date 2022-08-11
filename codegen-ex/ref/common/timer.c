#include "timer.h"

// timer
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
    for (int i = 0; i < ITERATIONS - WARMUP; ++i) {
        double deltas[5] = {};
        for (int j = 0; j < 4; ++j) {
            deltas[j] = compute_micro_time_delta(&global_timer.times[WARMUP + i][j], &global_timer.times[WARMUP + i][j + 1]);
        }
        double total = compute_micro_time_delta(&global_timer.times[WARMUP + i][0], &global_timer.times[WARMUP + i][4]);
        printf("%s, %d, %lf, %lf, %lf, %lf, %lf\n", name, i, deltas[0], deltas[1], deltas[2], deltas[3], total);
    }
}