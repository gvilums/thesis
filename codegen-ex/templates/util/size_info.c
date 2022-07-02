#include "common.h"

#include <stdio.h>

int main() {
% for i in range(num_globals):
    printf("%lu\n", sizeof(global_${ i }_t));
% endfor
% for i in range(num_constants):
    printf("%lu\n", sizeof(constant_${ i }_t));
% endfor
% for i in range(num_inputs):
    printf("%lu\n", sizeof(input_${ i }_t));
% endfor
    printf("%lu\n", sizeof(output_t));
}