<%inherit file="host.cpp"/>

<%block name="top_level_decl">
void pipeline_reduce_combine(reduction_out_t* __restrict out_ptr, const reduction_out_t* __restrict in_ptr) {
    ${ reduction["combine"] }
}

void reduce_parallel(char* values, size_t count) {
    // todo decide if parallelism makes sense, based on size of output_t and count
    // for now, assume we always work in parallel
    auto num_threads = std::thread::hardware_concurrency();
    std::vector<std::thread> threads;
    std::vector<size_t> result_offsets;

    threads.reserve(num_threads);
    result_offsets.reserve(num_threads);

    size_t base_inputs = count / num_threads;
    size_t remaining_elems = count % num_threads;

    for (int i = 0; i < num_threads; ++i) {
        size_t elem_count = base_inputs + (i < remaining_elems);
        size_t local_offset = elem_count * i + (i >= remaining_elems) * remaining_elems;
        result_offsets.push_back(local_offset);
        threads.emplace_back([=] {
            for (size_t i = 1; i < elem_count; ++i) {
                pipeline_reduce_combine((output_t*)&values[align8(sizeof(output_t)) * local_offset], 
                    (output_t*)&values[align8(sizeof(output_t)) * (local_offset + i)]);
            }
        });
    }
    for (auto& t : threads) {
        t.join();
    }

    for (int i = 1; i < num_threads; ++i) {
        pipeline_reduce_combine((output_t*)&values[0], (output_t*)&values[align8(sizeof(output_t)) * result_offsets[i]]);
    }
}
</%block>

<%block name="compute_result">
void compute_final_result(struct dpu_set_t set, uint32_t nr_dpus, reduction_out_t* output) {
    timer_retrieve_data();
    struct dpu_set_t dpu;
    uint32_t dpu_id;
	char* outputs = (char*)malloc(align8(sizeof(output_t)) * nr_dpus);
    // DPU_FOREACH(set, dpu) {
    //     DPU_ASSERT(dpu_log_read(dpu, stdout));
    // }
    DPU_FOREACH(set, dpu, dpu_id) { DPU_ASSERT(dpu_prepare_xfer(dpu, &outputs[align8(sizeof(output_t)) * dpu_id])); }
    DPU_ASSERT(
        dpu_push_xfer(set, DPU_XFER_FROM_DPU, DPU_MRAM_HEAP_POINTER_NAME, 0, align8(sizeof(output_t)), DPU_XFER_DEFAULT));

    timer_start_combine();

    reduce_parallel(outputs, nr_dpus);
    // for (int i = 1; i < nr_dpus; ++i) {
        // pipeline_reduce_combine(&outputs[0], &outputs[i]);
    // }

    memcpy(output, &outputs[0], sizeof(output_t));
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
