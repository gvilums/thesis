
#include "common.h"

#include <assert.h>
#include <dpu.h>
#include <stdint.h>
#include <stdio.h>

#define DPU_BINARY "device"


void pipeline_reduce_combine(reduction_out_t* restrict out_ptr,
                             const reduction_out_t* restrict in_ptr);

void setup_inputs(struct dpu_set_t set,
                   uint32_t nr_dpus,
                   const input_t* input,
                   size_t elem_count
                   , const global_0_t* global_0
                   ) {
    struct dpu_set_t dpu;
    uint32_t dpu_id;

    size_t base_inputs = elem_count / nr_dpus;
    size_t remaining_elems = elem_count % nr_dpus;

    DPU_FOREACH(set, dpu, dpu_id) {
        size_t input_elem_count = base_inputs;
        if (dpu_id < remaining_elems) {
            input_elem_count += 1;
        }

        size_t local_offset = input_elem_count * dpu_id;
        if (dpu_id >= remaining_elems) {
            local_offset += remaining_elems;
        }

        DPU_ASSERT(dpu_prepare_xfer(dpu, (void*)&input[local_offset]));
    }
    DPU_ASSERT(dpu_push_xfer(set, DPU_XFER_TO_DPU, "element_input_buffer", 0, sizeof(input_t) * (base_inputs + 1), DPU_XFER_DEFAULT));

    uint8_t globals_data_less[GLOBALS_SIZE_ALIGNED];
    memcpy(&globals_data_less[0], &base_inputs, sizeof(elem_count_t));

    memcpy(&globals_data_less[GLOBAL_0_OFFSET], global_0, sizeof(global_0_t));


    /*
    memcpy(&globals_data_less[GLOBAL_0_OFFSET], global_0, sizeof(global_0_t));
    memcpy(&globals_data_less[GLOBAL_1_OFFSET], global_1, sizeof(global_1_t));
    ...
    */

    uint8_t globals_data_more[GLOBALS_SIZE_ALIGNED];
    memcpy(globals_data_more, globals_data_less, sizeof(globals_data_more));
    *((uint32_t*)globals_data_more) += 1;

    /*
        insertion point for initialization of globals
    */

    DPU_FOREACH(set, dpu, dpu_id) {
        if (dpu_id < remaining_elems) {
            DPU_ASSERT(dpu_prepare_xfer(dpu, (void*)globals_data_more));
        } else {
            DPU_ASSERT(dpu_prepare_xfer(dpu, (void*)globals_data_less));
        }
    }
    DPU_ASSERT(dpu_push_xfer(set, DPU_XFER_TO_DPU, "globals_input_buffer", 0, GLOBALS_SIZE_ALIGNED, DPU_XFER_DEFAULT));
}

void compute_final_result(struct dpu_set_t set, uint32_t nr_dpus, reduction_out_t* output) {
    struct dpu_set_t dpu;
    uint32_t dpu_id;
    reduction_out_t outputs[nr_dpus];
    DPU_FOREACH(set, dpu, dpu_id) { DPU_ASSERT(dpu_prepare_xfer(dpu, &outputs[dpu_id])); }
    DPU_ASSERT(
        dpu_push_xfer(set, DPU_XFER_FROM_DPU, "reduction_output", 0, sizeof(reduction_out_t), DPU_XFER_DEFAULT));


    for (int i = 1; i < nr_dpus; ++i) {
        pipeline_reduce_combine(&outputs[0], &outputs[i]);
    }

    memcpy(output, &outputs[0], sizeof(outputs[0]));
}

int process(reduction_out_t* output, const input_t* input, size_t elem_count , const global_0_t* global_0) {
    struct dpu_set_t set, dpu;
    uint32_t nr_dpus;

    DPU_ASSERT(dpu_alloc(DPU_ALLOCATE_ALL, "backend=simulator", &set));
    DPU_ASSERT(dpu_load(set, DPU_BINARY, NULL));
    DPU_ASSERT(dpu_get_nr_dpus(set, &nr_dpus));

    setup_inputs(set, nr_dpus, input, elem_count , global_0);

    DPU_ASSERT(dpu_launch(set, DPU_SYNCHRONOUS));
    compute_final_result(set, nr_dpus, output);

    DPU_ASSERT(dpu_free(set));
    return 0;
}

void pipeline_reduce_combine(reduction_out_t* restrict out_ptr, const reduction_out_t* restrict in_ptr) {
    reduction_out_t in;
    reduction_out_t out;
    memcpy(&in, in_ptr, sizeof(in));
    memcpy(&out, out_ptr, sizeof(out));
    {
        out += in;

    }
    memcpy(out_ptr, &out, sizeof(out));
}
