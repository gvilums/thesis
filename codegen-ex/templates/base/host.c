#include "common.h"

#include <assert.h>
#include <dpu.h>
#include <stdint.h>
#include <stdio.h>

#define DPU_BINARY "device"

<%def name="param_decl()">
% for i in range(0, len(in_stage["inputs"])):
, const input_${ i }_t* input_${ i }\
% endfor
, size_t elem_count\
% for global_value in pipeline["globals"]:
, const global_${ loop.index }_t* global_${ loop.index }\
% endfor
</%def>

<%def name="param_use()">
% for i in range(0, len(in_stage["inputs"])):
, input_${ i }\
% endfor
    , elem_count\
% for global_value in pipeline["globals"]:
, global_${ loop.index }\
% endfor
</%def>

<%block name="top_level_decl"/>

void setup_inputs(struct dpu_set_t set, uint32_t nr_dpus ${ param_decl() }) {
    struct dpu_set_t dpu;
    uint32_t dpu_id;

    size_t base_inputs = elem_count / nr_dpus;
    size_t remaining_elems = elem_count % nr_dpus;

    size_t dpu_offsets[nr_dpus];
    DPU_FOREACH(set, dpu, dpu_id) {
        size_t input_elem_count = base_inputs;
        if (dpu_id < remaining_elems) {
            input_elem_count += 1;
        }
        size_t local_offset = input_elem_count * dpu_id;
        if (dpu_id >= remaining_elems) {
            local_offset += remaining_elems;
        }
        dpu_offsets[dpu_id] = local_offset;
    }

% for i in range(0, len(in_stage["inputs"])):
    {
        DPU_FOREACH(set, dpu, dpu_id) {
            DPU_ASSERT(dpu_prepare_xfer(dpu, (void*)&input_${ i }[dpu_offsets[dpu_id]]));
        }
        size_t aligned_max_size = ((sizeof(input_${ i }_t) * base_inputs) | 7) + 1;
        DPU_ASSERT(dpu_push_xfer(set, DPU_XFER_TO_DPU, "element_input_buffer_${ i }", 0, aligned_max_size, DPU_XFER_DEFAULT));
    }
%endfor

    uint8_t globals_data_less[GLOBALS_SIZE_ALIGNED];
    memcpy(&globals_data_less[0], &base_inputs, sizeof(elem_count_t));

% for global_value in pipeline["globals"]:
    memcpy(&globals_data_less[GLOBAL_${ loop.index }_OFFSET], global_${ loop.index }, sizeof(global_${ loop.index }_t));
% endfor

    uint8_t globals_data_more[GLOBALS_SIZE_ALIGNED];
    memcpy(globals_data_more, globals_data_less, sizeof(globals_data_more));
    *((uint32_t*)globals_data_more) += 1;

    DPU_FOREACH(set, dpu, dpu_id) {
        if (dpu_id < remaining_elems) {
            DPU_ASSERT(dpu_prepare_xfer(dpu, (void*)globals_data_more));
        } else {
            DPU_ASSERT(dpu_prepare_xfer(dpu, (void*)globals_data_less));
        }
    }
    DPU_ASSERT(dpu_push_xfer(set, DPU_XFER_TO_DPU, "globals_input_buffer", 0, GLOBALS_SIZE_ALIGNED, DPU_XFER_DEFAULT));
}

<%block name="compute_result"/>

<%block name="process"/>