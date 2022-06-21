<%inherit file="host.c"/>

<%block name="top_level_decl">
</%block>

<%block name="compute_result">
size_t compute_final_result(struct dpu_set_t set, uint32_t nr_dpus, output_t** output) {
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

    output_t* output_data_buffers[nr_dpus];
    DPU_FOREACH(set, dpu, dpu_id) { 
        output_data_buffers[dpu_id] = malloc(sizeof(output_t) * max_output_elems);
        DPU_ASSERT(dpu_prepare_xfer(dpu, output_data_buffers[dpu_id])); 
    }


    DPU_ASSERT(
        dpu_push_xfer(set, DPU_XFER_FROM_DPU, "element_output_buffer", 0, sizeof(output_t) * max_output_elems, DPU_XFER_DEFAULT));

    output_t* final_output = malloc(sizeof(output_t) * total_output_elems);

    size_t offset = 0;
    for (int i = 0; i < nr_dpus; ++i) {
        // for (int j = 0; j < output_elem_counts[i]; ++j) {
        //     printf("%u ", output_data_buffers[i][j]);
        // }
        // puts("\n");
        memcpy(&final_output[offset], output_data_buffers[i], sizeof(output_t) * output_elem_counts[i]);
        offset += output_elem_counts[i];
        free(output_data_buffers[i]);
    }
    *output = final_output;
    return total_output_elems;
}
</%block>

<%block name="process">
size_t process(output_t** output ${ parent.param_decl() }) {
    struct dpu_set_t set, dpu;
    uint32_t nr_dpus;

    DPU_ASSERT(dpu_alloc(DPU_ALLOCATE_ALL, "backend=simulator", &set));
    DPU_ASSERT(dpu_load(set, DPU_BINARY, NULL));
    DPU_ASSERT(dpu_get_nr_dpus(set, &nr_dpus));

    setup_inputs(set, nr_dpus ${ parent.param_use() });

    DPU_ASSERT(dpu_launch(set, DPU_SYNCHRONOUS));
    size_t output_elems = compute_final_result(set, nr_dpus, output);

    DPU_ASSERT(dpu_free(set));
    return output_elems;
}
</%block>