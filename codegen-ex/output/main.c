#include "host.h"

#include <stdio.h>
#include <stdlib.h>

// #define EXAMPLE_INPUT
// #define HISTOGRAM
#define FILTER_MULTIPLES

#ifdef EXAMPLE_INPUT
int main() {
    const size_t elem_count = 1000;
    input_t* input = malloc(sizeof(input_t) * elem_count);
    for (int i = 0; i < elem_count; ++i) {
        input[i][0] = 1;
        input[i][1] = 2;
    }
    uint32_t global_0 = 1;
    reduction_out_t output;
    process(&output, input, elem_count, &global_0);
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
    const size_t elem_count = 40;
    input_t* input = malloc(sizeof(input_t) * elem_count);
    for (int i = 0; i < elem_count; ++i) {
        input[i] = i;
    }
    output_t* output;
    uint32_t factor = 2;
    size_t result_count = process(&output, input, elem_count, &factor);
    printf("%lu\n", result_count);
    for (uint32_t i = 0; i < result_count; ++i) {
        printf("%u ", output[i]);
        if (output[i] != factor * i) {
            puts("error");
        }
    }
    puts("\n");
}
#endif