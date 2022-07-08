<%inherit file="host.cpp"/>

<%block name="top_level_decl">
void copy_parallel(output_t* target, output_t* src, size_t nr_dpus, elem_count_t* output_elem_counts, elem_count_t max_output_elems) {
    size_t offsets[nr_dpus] = {};
    for (int i = 1; i < nr_dpus; ++i) {
        offsets[i] = offsets[i - 1] + output_elem_counts[i - 1];
    }
    // std::for_each(
    //     // std::execution::par_unseq, 
    //     &indices[0], &indices[nr_dpus], [&](size_t i) {
    //     memcpy(&target[offsets[i]], &src[i * max_output_elems], sizeof(output_t) * output_elem_counts[i]);
    // });

    // todo decide if parallelism makes sense, based on size of output_t and count
    // for now, assume we always work in parallel
    auto num_threads = std::thread::hardware_concurrency();
    std::vector<std::thread> threads;

    threads.reserve(num_threads);

    size_t base_inputs = nr_dpus / num_threads;
    size_t remaining_elems = nr_dpus % num_threads;

    for (int i = 0; i < num_threads; ++i) {
        size_t elem_count = base_inputs + (i < remaining_elems);
        size_t local_offset = elem_count * i + (i >= remaining_elems) * remaining_elems;
        threads.emplace_back([=,&offsets] {
            for (size_t j = local_offset; j < local_offset + elem_count; ++j) {
                memcpy(&target[offsets[j]], &src[j * max_output_elems], sizeof(output_t) * output_elem_counts[j]);
            }
        });
    }
    for (auto& t : threads) {
        t.join();
    }
}
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
