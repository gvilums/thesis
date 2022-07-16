<%inherit file="device.c"/>


<%block name="main_process">
    size_t reduction_idx = index % NR_REDUCTION_VARS;

    // init local reduction variable
    if (index == reduction_idx) {
        memcpy(&reduction_vars[reduction_idx], &${ reduction["identity"] }, sizeof(${ reduction["identity"] }));
    }
    barrier_wait(&reduction_init_barrier);

    for (size_t i = 0; i < input_elem_count; ++i) {
        ${ parent.call_with_inputs("pipeline", "reduction_idx") }
        ${ parent.advance_readers() }
    }

    // only main tasklet performs final reduction
    barrier_wait(&final_reduction_barrier);
    if (index == 0) {
        reduce();
        puts("dpu ok");
    }
</%block>

<%block name="global_decl">
#if NR_REDUCTION_VARS < NR_TASKLETS
#define SYNCHRONIZE_REDUCTION
#elif NR_REDUCTION_VARS > NR_TASKLETS
#error Cannot have more reduction variables than tasklets
#endif

// reduction values and helpers
__host reduction_out_t reduction_vars[NR_REDUCTION_VARS];
__atomic_bit uint8_t reduction_mutexes[NR_REDUCTION_VARS];

__mram_noinit reduction_out_t reduction_output;

// various barriers
BARRIER_INIT(setup_barrier, NR_TASKLETS);
BARRIER_INIT(reduction_init_barrier, NR_TASKLETS);
BARRIER_INIT(final_reduction_barrier, NR_TASKLETS);
</%block>

<%block name="function_decl">
void pipeline_reduce(reduction_out_t* restrict out_ptr, const reduction_in_t* restrict in_ptr) {
    ${ reduction["program"] }
}

void pipeline_reduce_combine(reduction_out_t* restrict out_ptr, const reduction_out_t* restrict in_ptr) {
    ${ reduction["combine"] }
}

void reduce() {
    for (size_t i = 1; i < NR_REDUCTION_VARS; ++i) {
        pipeline_reduce_combine(&reduction_vars[0], &reduction_vars[i]);
    }
}

void pipeline(uint32_t reduction_idx\
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
        return;
    }
    % endif
% endfor

// LOCAL REDUCTION
#ifdef SYNCHRONIZE_REDUCTION
    mutex_lock(&reduction_mutexes[reduction_idx]);
#endif

    pipeline_reduce(&reduction_vars[reduction_idx], &tmp_${ reduction["input_idx"] });

#ifdef SYNCHRONIZE_REDUCTION
    mutex_unlock(&reduction_mutexes[reduction_idx]);
#endif
}


</%block>