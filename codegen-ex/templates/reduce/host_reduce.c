<%inherit file="host.c"/>

<%block name="top_level_decl">
void pipeline_reduce_combine(reduction_out_t* restrict out_ptr, const reduction_out_t* restrict in_ptr) {
    ${ reduction["combine"] }
}
</%block>

<%block name="compute_result">
void compute_final_result(struct dpu_set_t set, uint32_t nr_dpus, reduction_out_t* output) {
    timer_retrieve_data();
    struct dpu_set_t dpu;
    uint32_t dpu_id;
	reduction_out_t* outputs = (reduction_out_t*)malloc(sizeof(reduction_out_t) * nr_dpus);
    // DPU_FOREACH(set, dpu) {
    //     DPU_ASSERT(dpu_log_read(dpu, stdout));
    // }
    DPU_FOREACH(set, dpu, dpu_id) { DPU_ASSERT(dpu_prepare_xfer(dpu, &outputs[dpu_id])); }
    DPU_ASSERT(
        dpu_push_xfer(set, DPU_XFER_FROM_DPU, "reduction_vars", 0, sizeof(reduction_out_t), DPU_XFER_DEFAULT));

    timer_start_combine();

    for (int i = 1; i < nr_dpus; ++i) {
        pipeline_reduce_combine(&outputs[0], &outputs[i]);
    }

    memcpy(output, &outputs[0], sizeof(outputs[0]));
	free(outputs);
}
</%block>

<%block name="process">
int process(output_t* output ${ parent.param_decl() }) {
    struct dpu_set_t set;
    uint32_t nr_dpus;

#ifdef SIMULATOR
    DPU_ASSERT(dpu_alloc(DPU_ALLOCATE_ALL, "backend=simulator", &set));
#else
    DPU_ASSERT(dpu_alloc(DPU_ALLOCATE_ALL, NULL, &set));
#endif

    DPU_ASSERT(dpu_load(set, DPU_BINARY, NULL));
    DPU_ASSERT(dpu_get_nr_dpus(set, &nr_dpus));

    timer_start_transfer();

    setup_inputs(set, nr_dpus ${ parent.param_use() });

    timer_launch_dpus();

    DPU_ASSERT(dpu_launch(set, DPU_SYNCHRONOUS));
    compute_final_result(set, nr_dpus, output);

    timer_finish();

    DPU_ASSERT(dpu_free(set));
    return 0;
}
</%block>
