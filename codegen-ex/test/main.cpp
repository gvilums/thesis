#include "host.hpp"

#include <stdio.h>
#include <stdlib.h>

// #define COMBINE_ADD
// #define HISTOGRAM_SMALL
// #define HISTOGRAM_LARGE
// #define FILTER_MULTIPLES
// #define SUM_REDUCE
// #define MAP_ONLY
// #define VECTOR_ADD

#ifndef SIZE_FACTOR
#define SIZE_FACTOR 10
#endif

#ifdef COMBINE_ADD
int main() {
    // puts("start test...");
    const size_t elem_count = 1000 * SIZE_FACTOR;
    input_0_t* input_0 = (input_0_t*)malloc(sizeof(input_0_t) * elem_count);
    input_1_t* input_1 = (input_1_t*)malloc(sizeof(input_1_t) * elem_count);
    for (size_t i = 0; i < elem_count; ++i) {
        input_0[i] = 1;
        input_1[i] = 2;
    }
    uint32_t global_0 = 1;
    reduction_out_t output;
    for (size_t i = 0; i < ITERATIONS; ++i) {
        process(&output, input_0, input_1, elem_count, &global_0);
    }

    printf("combine_add, ");
    timer_print_summary();

    if (output == 4 * elem_count) {
        // puts("sum_reduce: ok");
        return 0;
    } else {
        // puts("sum_reduce: ERROR");
        return 1;
    }
}
#endif

#ifdef HISTOGRAM_SMALL
int main() {
    // puts("start test...");
    const size_t elem_count = 1000 * SIZE_FACTOR;
    input_0_t* input = (input_0_t*)malloc(sizeof(input_0_t) * elem_count);
	uint32_t val = 1;
    for (size_t i = 0; i < elem_count; ++i) {
        input[i] = val;
		val = (16807 * val) % (~0 - 1);
    }
    reduction_out_t output;
    for (size_t i = 0; i < ITERATIONS; ++i) {
        process(&output, input, elem_count);
    }

    printf("histogram_small, ");
    timer_print_summary();

    // for (uint32_t i = 0; i < 256; ++i) {
        // printf("%u: %u\n", i, output[i]);
    // }
    return 0;
}
#endif

#ifdef HISTOGRAM_LARGE
int main() {
    // puts("start test...");
    const size_t elem_count = 1000 * SIZE_FACTOR;
    input_0_t* input = (input_0_t*)malloc(sizeof(input_0_t) * elem_count);
    uint32_t val = 1;
    for (uint32_t i = 0; i < elem_count; ++i) {
        input[i] = val;
        val = (16807 * val) % (~0 - 1);
    }
    reduction_out_t output;
    for (size_t i = 0; i < ITERATIONS; ++i) {
        process(&output, input, elem_count);
    }

    printf("histogram_large, ");
    timer_print_summary();

    // for (uint32_t i = 0; i < 2048; ++i) {
    //     printf("%u: %u\n", i, output[i]);
    // }
}
#endif

#ifdef SELECT
int main() {
    // puts("start test...");
    const size_t elem_count = 1000 * SIZE_FACTOR;
    input_0_t* input = (input_0_t*)malloc(sizeof(input_0_t) * elem_count);
    for (size_t i = 0; i < elem_count; ++i) {
        input[i] = i;
    }
    output_t* output;
    size_t result_count = 0;
    int err = 0;
    for (size_t i = 0; i < ITERATIONS; ++i) {
        result_count = process(&output, input, elem_count);
    }

    printf("select, ");
    timer_print_summary();

    for (uint64_t i = 0; i < result_count; ++i) {
        if (output[i] != i * 2) {
            printf("expected %lu, got %lu\n", 2 * i, output[i]);
            ++err;
        }
    }
    if (err) {
        return 1;
        // puts("filter_multiples: ERROR");
    } else {
        return 0;
        // puts("filter_multiples: ok");
    }
}
#endif

#ifdef FILTER_MULTIPLES
int main() {
    // puts("start test...");
    const size_t elem_count = 1000 * SIZE_FACTOR;
    input_0_t* input = (input_0_t*)malloc(sizeof(input_0_t) * elem_count);
    for (size_t i = 0; i < elem_count; ++i) {
        input[i] = i;
    }
    output_t* output;
    uint32_t factor = 2;
    size_t result_count = 0;
    int err = 0;
    for (size_t i = 0; i < ITERATIONS; ++i) {
        result_count = process(&output, input, elem_count, &factor);
    }

    printf("filter_multiples, ");
    timer_print_summary();

    for (uint64_t i = 0; i < result_count; ++i) {
        if (output[i] != i * factor) {
            printf("expected %lu, got %lu\n", factor * i, output[i]);
            ++err;
        }
    }
    if (err) {
        return 1;
        // puts("filter_multiples: ERROR");
    } else {
        return 0;
        // puts("filter_multiples: ok");
    }
}
#endif

#ifdef PRIME_SIEVE
int main() {
    // puts("start test...");
    const size_t elem_count = 10 * SIZE_FACTOR;
    input_0_t* input = (input_0_t*)malloc(sizeof(input_0_t) * elem_count);
    for (size_t i = 0; i < elem_count; ++i) {
        input[i] = i;
    }
    output_t* output;
    size_t result_count = 0;
    for (size_t i = 0; i < ITERATIONS; ++i) {
        result_count = process(&output, input, elem_count);
    }

    printf("prime_sieve, ");
    timer_print_summary();

}
#endif

#ifdef SUM_REDUCE
int main() {
    // puts("start test...");
    const size_t elem_count = 1000 * SIZE_FACTOR;
    input_0_t* input = (input_0_t*)malloc(sizeof(input_0_t) * elem_count);
    for (size_t i = 0; i < elem_count; ++i) {
        input[i] = 1;
    }
    reduction_out_t output;
    for (size_t i = 0; i < ITERATIONS; ++i) {
        process(&output, input, elem_count);
    }

    printf("sum_reduce, ");
    timer_print_summary();

    if (output == elem_count) {
        // puts("sum_reduce: ok");
        return 0;
    } else {
        // puts("sum_reduce: ERROR");
        return 1;
    }
}
#endif

#ifdef MAP_ONLY
int main() {
    // puts("start test...");
    const size_t elem_count = 1000 * SIZE_FACTOR;
    input_0_t* input = (input_0_t*)malloc(sizeof(input_0_t) * elem_count);
    for (size_t i = 0; i < elem_count; ++i) {
        input[i] = i;
    }
    output_t* output;
    global_0_t factor = 2;
    size_t result_count = 0;
    for (size_t i = 0; i < ITERATIONS; ++i) {
        result_count = process(&output, input, elem_count, &factor);
    }

    printf("map_only, ");
    timer_print_summary();

    for (uint32_t i = 0; i < result_count; ++i) {
        if (output[i] != i * factor) {
            // puts("map_only: ERROR");
            return 1;
        }
    }
    // puts("map_only: ok");
    return 0;
}
#endif

#ifdef VECTOR_ADD
int main() {
    // puts("start test...");
    const size_t elem_count = 1000 * SIZE_FACTOR;
    input_0_t* input_0 = (input_0_t*)malloc(sizeof(input_0_t) * elem_count);
    input_1_t* input_1 = (input_1_t*)malloc(sizeof(input_1_t) * elem_count);
    for (size_t i = 0; i < elem_count; ++i) {
        input_0[i] = 1;
        input_1[i] = 2;
    }
    size_t count = 0;
    output_t* output;
    for (size_t i = 0; i < ITERATIONS; ++i) {
        count = process(&output, input_0, input_1, elem_count);
    }

    printf("vector_add, ");
    timer_print_summary();

	if (count != elem_count) {
		// puts("vector_add: ERROR");
		return 1;
	}
    for (size_t i = 0; i < count; ++i) {
        if (output[i] != 3) {
            // puts("vector_add: ERROR");
            return 1;
        }
    }
	// puts("vector_add: ok");
    return 0;
}
#endif

#ifdef DEDUP
int main() {
    // puts("start test...");
    const size_t elem_count = 30 * SIZE_FACTOR;
    input_0_t* input = (input_0_t*)malloc(sizeof(input_0_t) * elem_count);
    uint32_t val = 1;
    for (size_t i = 0; i < elem_count; ++i) {
        for (size_t j = 0; j < sizeof(input_0_t); ++j) {
            input[i][j] = (uint8_t)val;
            val = (16807 * val) % (~0 - 1);
        }
    }
    output_t* output;
    size_t result_count = 0;
    for (size_t i = 0; i < ITERATIONS; ++i) {
        result_count = process(&output, input, elem_count);
    }

    printf("dedup, ");
    timer_print_summary();

    if (result_count != elem_count) {
        return 1;
        // puts("dedup: ERROR (invalid result size)");
    }
    for (size_t i = 0; i < elem_count; ++i) {
        for (size_t j = 1; j < sizeof(input_0_t); ++j) {
            if (output[i][j] != 0 && output[i][j] == output[i][j - 1]) {
                // puts("dedup: ERROR");
                return 1;
            }
        }
    }
    // puts("dedup: ok");
    return 0;
}
#endif

#ifdef MAXIMA
int main() {
    // puts("start test...");
    const size_t elem_count = 500 * SIZE_FACTOR;
    input_0_t* input = (input_0_t*)malloc(sizeof(input_0_t) * elem_count);
    uint32_t val = 1;
    for (size_t i = 0; i < elem_count; ++i) {
        input[i] = val;
        val = (16807 * val) % (~0 - 1);
    }
    output_t output;
    for (size_t i = 0; i < ITERATIONS; ++i) {
        process(&output, input, elem_count);
    }

    printf("maxima, ");
    timer_print_summary();

    // puts("maxima: ok");
    return 0;
}
#endif
