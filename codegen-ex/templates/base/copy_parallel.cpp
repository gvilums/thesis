#include "common.h"

#include <algorithm>
#include <execution>
#include <cstring>

extern "C" void copy_parallel(output_t* target, output_t* src, size_t nr_dpus, size_t* output_elem_counts, size_t max_output_elems) {
    size_t offsets[nr_dpus] = {};
    size_t indices[nr_dpus] = {};
    for (int i = 1; i < nr_dpus; ++i) {
        indices[i] = i;
        offsets[i] = offsets[i - 1] + output_elem_counts[i];
    }
    std::for_each(std::execution::par_unseq, &indices[0], &indices[nr_dpus], [&](size_t i) {
        memcpy(&target[offsets[i]], &src[i * max_output_elems], sizeof(output_t) * output_elem_counts[i]);
    });
}