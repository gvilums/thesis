<%inherit file="host.c"/>

<%block name="top_level_decl">
</%block>

<%block name="compute_result">
size_t compute_final_result(struct dpu_set_t set, uint32_t nr_dpus, output_t** output) {
    timer_retrieve_data();
    struct dpu_set_t dpu;
    uint32_t dpu_id;
    elem_count_t output_elem_counts[nr_dpus];
    DPU_FOREACH(set, dpu, dpu_id) { 
        DPU_ASSERT(dpu_prepare_xfer(dpu, &output_elem_counts[dpu_id])); 
    }
    DPU_ASSERT(
        dpu_push_xfer(set, DPU_XFER_FROM_DPU, "total_output_elems", 0, sizeof(uint32_t), DPU_XFER_DEFAULT));

    size_t total_output_elems = 0;
    elem_count_t max_output_elems = 0;
    for (int i = 0; i < nr_dpus; ++i) {
        total_output_elems += output_elem_counts[i];
        if (output_elem_counts[i] > max_output_elems) {
            max_output_elems = output_elem_counts[i];
        }
    }
    // align to 8-byte multiple for transfer
    max_output_elems = ((max_output_elems - 1) | 7) + 1;

    output_t* temp_output_buffer = (output_t*)malloc(nr_dpus * max_output_elems * sizeof(output_t));
    DPU_FOREACH(set, dpu, dpu_id) { 
        DPU_ASSERT(dpu_prepare_xfer(dpu, &temp_output_buffer[dpu_id * max_output_elems])); 
    }

    timer_start_combine();

    DPU_ASSERT(
        dpu_push_xfer(set, DPU_XFER_FROM_DPU, "element_output_buffer", 0, sizeof(output_t) * max_output_elems, DPU_XFER_DEFAULT));

    output_t* final_output = (output_t*)malloc(sizeof(output_t) * total_output_elems);

    copy_parallel(final_output, temp_output_buffer, nr_dpus, &output_elem_counts[0], max_output_elems);
    free(temp_output_buffer);
    *output = final_output;
    return total_output_elems;
}
</%block>

<%block name="process">
size_t process(output_t** output ${ parent.param_decl() }) {
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

    size_t output_elems = compute_final_result(set, nr_dpus, output);

    timer_finish();

    DPU_ASSERT(dpu_free(set));
    return output_elems;
}
</%block>
