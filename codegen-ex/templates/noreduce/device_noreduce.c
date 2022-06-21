<%inherit file="device.c"/>



<%block name="main_process">
% if "no_filter" in pipeline:
    output_elems[index] = input_elem_count;
% else:
    output_elems[index] = 0;
    for (size_t i = 0; i < input_elem_count; ++i) {
        output_elems[index] += ${ parent.call_with_inputs("pipeline_eval_condition", "0") }
        ${ parent.advance_readers() }
    }
% endif

    barrier_wait(&output_offset_compute);

    size_t output_offset = 0;
    for (int i = 0; i < index; ++i) {
        output_offset += output_elems[i];
    }

    if (index == NR_TASKLETS - 1) {
        total_output_elems = output_offset + output_elems[index];
    }

    // reset readers
% for i in range(0, len(in_stage["inputs"])):
    current_read_${ i } = seqread_init(
        local_cache_${ i }, &element_input_buffer_${ i }[local_offset * sizeof(input_${ i }_t)], &sr_${ i });
% endfor

    init_output_writer(output_offset);
    output_t* current_write = get_output_place();
    for (size_t i = 0; i < input_elem_count; ++i) {
        int result = pipeline(current_write\
% for i in range(0, len(in_stage["inputs"])):
, current_read_${ i }\
% endfor
);
        ${ parent.advance_readers() }
        if (result) {
            confirm_write();
            current_write = get_output_place();
        }
    }
    flush_outputs();
</%block>



<%block name="global_decl">
static_assert(sizeof(output_t) % 8 == 0, "output type must be 8-byte aligned");
// data output
__mram_noinit uint8_t element_output_buffer[INPUT_BUF_SIZE];
__host elem_count_t total_output_elems;


// various barriers
BARRIER_INIT(setup_barrier, NR_TASKLETS);
BARRIER_INIT(output_offset_compute, NR_TASKLETS);

// output handling
#define MRAM_ALIGN 8
#define LOCAL_OUTBUF_SIZE (1 << 8)

// element count for each tasklet
uint32_t output_elems[NR_TASKLETS];

__dma_aligned uint8_t local_output_buffers[NR_TASKLETS][LOCAL_OUTBUF_SIZE];
uint32_t local_outbuf_sizes[NR_TASKLETS];
uint32_t current_output_offsets[NR_TASKLETS];
</%block>



<%block name="function_decl">
void flush_outputs() {
    uint32_t i = me();
    mram_write(&local_output_buffers[i][0], &element_output_buffer[current_output_offsets[i]], local_outbuf_sizes[i]);
    // advance offset by size of data we just copied
    current_output_offsets[i] += local_outbuf_sizes[i];
    // reset the local output buffer to size 0
    local_outbuf_sizes[i] = 0;
}

output_t* get_output_place() {
    uint32_t i = me();

    // if adding another value of type output_t would overflow the buffer, flush
    if (local_outbuf_sizes[i] + sizeof(output_t) > LOCAL_OUTBUF_SIZE) {
        flush_outputs();
    }

    output_t* place = (output_t*)&local_output_buffers[i][local_outbuf_sizes[i]];
    return place;
}

void confirm_write() {
    local_outbuf_sizes[me()] += sizeof(output_t);
}

void init_output_writer(size_t element_offset) {
    uint32_t i = me();

    local_outbuf_sizes[i] = 0;
    current_output_offsets[i] = element_offset * sizeof(output_t);
}

int pipeline(output_t* data_out\
% for i in range(0, len(in_stage["inputs"])):
, input_${ i }_t* input_${ i }\
% endfor
) {
    stage_0_out_t tmp_0;
% for stage in stages:
    % if stage["kind"] != "filter":
    stage_${ stage["id"] }_out_t tmp_${ stage["id"] };
    % endif
% endfor

    pipeline_input(&tmp_0\
% for i in range(0, len(in_stage["inputs"])):
, input_${ i }\
% endfor
);

% for stage in stages:
    % if stage["kind"] == "map":
    // map
    stage_${ stage["id"] }(&tmp_${ stage["input_idx"] }, &tmp_${ stage["id"] });
    % elif stage["kind"] == "filter":
    // filter
    if (!stage_${ stage["id"] }(&tmp_${ stage["input_idx"] })) {
        return 0;
    }
    % endif
% endfor

    memcpy(data_out, &tmp_${ output["input_idx"] }, sizeof(output_t));
    return 1;
}

<%
    filter_eval_stages = []
    for stage in stages:
        filter_eval_stages.append(stage)
        if "last_filter" in stage:
            break
%>
% if "no_filter" not in pipeline: 
int pipeline_eval_condition(size_t dummy\
% for i in range(0, len(in_stage["inputs"])):
, input_${ i }_t* input_${ i }\
% endfor
) {
    stage_0_out_t tmp_0;
% for stage in stages:
    % if stage["kind"] != "filter":
    stage_${ stage["id"] }_out_t tmp_${ stage["id"] };
    % endif
% endfor

    pipeline_input(&tmp_0\
% for i in range(0, len(in_stage["inputs"])):
, input_${ i }\
% endfor
);

% for stage in filter_eval_stages:
    % if stage["kind"] == "map":
    // map
    stage_${ stage["id"] }(&tmp_${ stage["input_idx"] }, &tmp_${ stage["id"] });
    % elif stage["kind"] == "filter":
    // filter
    if (!stage_${ stage["id"] }(&tmp_${ stage["input_idx"] })) {
        return 0;
    }
    % endif
% endfor

    return 1;
}
% endif

</%block>