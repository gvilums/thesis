#include "host.h"

#include <stdio.h>
#include <stdlib.h>


int main() {
    const size_t elem_count = 1000;
    input_t* input = malloc(sizeof(input_t) * elem_count);
    for (int i = 0; i < elem_count; ++i) {
        input[i][0] = 1;
        input[i][1] = 2;
    }
    reduction_out_t output;
    process(&output, input, elem_count);
    printf("result: %u\n", output);
}