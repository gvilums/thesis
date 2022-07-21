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

void timer_print_summary(void) {
    if (iter != ITERATIONS) {
        puts("invalid timer state on timer_print_summary");
        exit(EXIT_FAILURE);
    }
    double avg_times[4];
    for (int i = 0; i < 4; ++i) {
        double sum = 0;
        for (int j = 0; j < ITERATIONS; ++j) {
            sum += compute_micro_time_delta(&global_timer.times[j][i], &global_timer.times[j][i + 1]);
        }
        sum /= ITERATIONS;
        avg_times[i] = sum;
    }

    double total = 0;
    for (int j = 0; j < ITERATIONS; ++j) {
        total += compute_micro_time_delta(&global_timer.times[j][0], &global_timer.times[j][4]);
    }
    total /= ITERATIONS;
// #ifdef PRINT_CSV
    printf("%lf, %lf, %lf, %lf, %lf\n", avg_times[0], avg_times[1], avg_times[2], avg_times[3], total);
// #else
//     printf("cpu -> dpu transfer %15lf us\n", avg_times[0]);
//     printf("dpu execution       %15lf us\n", avg_times[1]);
//     printf("dpu -> cpu transfer %15lf us\n", avg_times[2]);
//     printf("cpu final combine   %15lf us\n", avg_times[3]);
// 	printf("--------------------------------------\n");
//     printf("total time          %15lf us\n", total);
// #endif
}