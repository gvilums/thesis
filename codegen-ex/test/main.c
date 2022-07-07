#include "host.h"

#include <stdio.h>
#include <stdlib.h>

// #define COMBINE_ADD
// #define HISTOGRAM_SMALL
// #define HISTOGRAM_LARGE
// #define FILTER_MULTIPLES
// #define SUM_REDUCE
// #define MAP_ONLY
// #define VECTOR_ADD

#define SIZE_FACTOR 100

#ifdef COMBINE_ADD
int main() {
    const size_t elem_count = 1000 * SIZE_FACTOR;
    input_0_t* input_0 = malloc(sizeof(input_0_t) * elem_count);
    input_1_t* input_1 = malloc(sizeof(input_1_t) * elem_count);
    for (int i = 0; i < elem_count; ++i) {
        input_0[i] = 1;
        input_1[i] = 2;
    }
    uint32_t global_0 = 1;
    reduction_out_t output;
    for (int i = 0; i < ITERATIONS; ++i) {
        process(&output, input_0, input_1, elem_count, &global_0);
    }
    timer_print_summary();
    if (output == 4 * elem_count) {
        puts("sum_reduce: ok");
    } else {
        puts("sum_reduce: ERROR");
        return 1;
    }
}
#endif

#ifdef HISTOGRAM_SMALL
int main() {
    const size_t elem_count = 1000 * SIZE_FACTOR;
    input_0_t* input = malloc(sizeof(input_0_t) * elem_count);
	uint32_t val = 1;
    for (int i = 0; i < elem_count; ++i) {
        input[i] = val;
		val = (16807 * val) % (~0 - 1);
    }
    reduction_out_t output;
    for (int i = 0; i < ITERATIONS; ++i) {
        process(&output, input, elem_count);
    }
    timer_print_summary();
    // for (uint32_t i = 0; i < 256; ++i) {
        // printf("%u: %u\n", i, output[i]);
    // }
}
#endif

#ifdef HISTOGRAM_LARGE
int main() {
    const size_t elem_count = 1000 * SIZE_FACTOR;
    input_0_t* input = malloc(sizeof(input_0_t) * elem_count);
    uint32_t val = 1;
    for (uint32_t i = 0; i < elem_count; ++i) {
        input[i] = val;
        val = (16807 * val) % (~0 - 1);
    }
    reduction_out_t output;
    for (int i = 0; i < ITERATIONS; ++i) {
        process(&output, input, elem_count);
    }
    timer_print_summary();
    // for (uint32_t i = 0; i < 2048; ++i) {
    //     printf("%u: %u\n", i, output[i]);
    // }
}
#endif

#ifdef FILTER_MULTIPLES
int main() {
    const size_t elem_count = 1000 * SIZE_FACTOR;
    input_0_t* input = malloc(sizeof(input_0_t) * elem_count);
    for (int i = 0; i < elem_count; ++i) {
        input[i] = i;
    }
    output_t* output;
    uint32_t factor = 2;
    size_t result_count = 0;
    int err = 0;
    for (int i = 0; i < ITERATIONS; ++i) {
        result_count = process(&output, input, elem_count, &factor);
    }
    timer_print_summary();
    for (uint64_t i = 0; i < result_count; ++i) {
        if (output[i] != i * factor) {
            printf("expected %lu, got %lu\n", factor * i, output[i]);
            ++err;
        }
    }
    if (err) {
        puts("filter_multiples: ERROR");
    } else {
        puts("filter_multiples: ok");
    }
}
#endif

#ifdef SUM_REDUCE
int main() {
    const size_t elem_count = 1000 * SIZE_FACTOR;
    input_0_t* input = malloc(sizeof(input_0_t) * elem_count);
    for (int i = 0; i < elem_count; ++i) {
        input[i] = 1;
    }
    reduction_out_t output;
    for (int i = 0; i < ITERATIONS; ++i) {
        process(&output, input, elem_count);
    }
    timer_print_summary();
    if (output == elem_count) {
        puts("sum_reduce: ok");
    } else {
        puts("sum_reduce: ERROR");
        return 1;
    }
}
#endif

#ifdef MAP_ONLY
int main() {
    const size_t elem_count = 1000 * SIZE_FACTOR;
    input_0_t* input = malloc(sizeof(input_0_t) * elem_count);
    for (int i = 0; i < elem_count; ++i) {
        input[i] = i;
    }
    output_t* output;
    global_0_t factor = 2;
    size_t result_count = 0;
    for (int i = 0; i < ITERATIONS; ++i) {
        result_count = process(&output, input, elem_count, &factor);
    }
    timer_print_summary();
    for (uint32_t i = 0; i < result_count; ++i) {
        if (output[i] != i * factor) {
            puts("map_only: ERROR");
            return 1;
        }
    }
    puts("map_only: ok");
}
#endif

#ifdef VECTOR_ADD
int main() {
    const size_t elem_count = 1000 * SIZE_FACTOR;
    input_0_t* input_0 = malloc(sizeof(input_0_t) * elem_count);
    input_1_t* input_1 = malloc(sizeof(input_1_t) * elem_count);
    for (int i = 0; i < elem_count; ++i) {
        input_0[i] = 1;
        input_1[i] = 2;
    }
    size_t count = 0;
    output_t* output;
    for (int i = 0; i < ITERATIONS; ++i) {
        count = process(&output, input_0, input_1, elem_count);
    }
    timer_print_summary();
	if (count != elem_count) {
		puts("vector_add: ERROR");
		return 1;
	}
    for (size_t i = 0; i < count; ++i) {
        if (output[i] != 3) {
            puts("vector_add: ERROR");
            return 1;
        }
    }
	puts("vector_add: ok");
    return 0;
}
#endif
