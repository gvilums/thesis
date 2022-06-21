#include "host.h"

#include <stdio.h>
#include <stdlib.h>

#define EXAMPLE_INPUT
// #define HISTOGRAM
// #define FILTER_MULTIPLES
// #define SUM_REDUCE
// #define MAP_ONLY

#ifdef EXAMPLE_INPUT
int main() {
    const size_t elem_count = 1000;
    input_0_t* input_0 = malloc(sizeof(input_0_t) * elem_count);
    input_1_t* input_1 = malloc(sizeof(input_1_t) * elem_count);
    for (int i = 0; i < elem_count; ++i) {
        input_0[i] = 1;
        input_1[i] = 2;
    }
    uint32_t global_0 = 1;
    reduction_out_t output;
    process(&output, input_0, input_1, elem_count, &global_0);
    printf("result: %u\n", output);
}
#endif

#ifdef HISTOGRAM
int main() {
    const size_t elem_count = 130;
    input_t* input = malloc(sizeof(input_t) * elem_count);
    for (int i = 0; i < elem_count; ++i) {
        input[i] = i;
    }
    reduction_out_t output;
    process(&output, input, elem_count);
    for (uint32_t i = 0; i < 256; ++i) {
        printf("%u: %u\n", i, output[i]);
    }
}
#endif


#ifdef FILTER_MULTIPLES
int main() {
    const size_t elem_count = 10000;
    input_t* input = malloc(sizeof(input_t) * elem_count);
    for (int i = 0; i < elem_count; ++i) {
        input[i] = i;
    }
    output_t* output;
    uint32_t factor = 2;
    size_t result_count = process(&output, input, elem_count, &factor);
    printf("number of results: %lu\n", result_count);
    for (uint32_t i = 0; i < result_count; ++i) {
        if (output[i] != i * factor) {
            puts("error");
            return 1;
        }
    }
}
#endif

#ifdef SUM_REDUCE
int main() {
    const size_t elem_count = 1000;
    input_t* input = malloc(sizeof(input_t) * elem_count);
    for (int i = 0; i < elem_count; ++i) {
        input[i] = i;
    }
    reduction_out_t output;
    process(&output, input, elem_count);
    printf("result: %u\n", output);
}
#endif

#ifdef MAP_ONLY
int main() {
    const size_t elem_count = 100;
    input_t* input = malloc(sizeof(input_t) * elem_count);
    for (int i = 0; i < elem_count; ++i) {
        input[i] = i;
    }
    output_t* output;
    global_0_t factor = 2;
    size_t result_count = process(&output, input, elem_count, &factor);
    printf("number of results: %lu\n", result_count);
    for (uint32_t i = 0; i < result_count; ++i) {
        if (output[i] != i * factor) {
            puts("error");
            return 1;
        }
    }
    puts("ok");
}
#endif