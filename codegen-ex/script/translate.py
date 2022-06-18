map_template = """
void {func_name}(const {in_type}* restrict in_ptr, {out_type}* restrict out_ptr) {{
    {in_type} in;
    {out_type} out;
    memcpy(&in, in_ptr, sizeof(in));
    {{
        // MAP PROGRAM
        {program}
    }}
    memcpy(out_ptr, &out, sizeof(out));
}}
"""

filter_template = """
int {func_name}(const {in_type}* restrict in_ptr) {{
    {in_type} in;
    memcpy(&in, in_ptr, sizeof(in));
    {{
        // FILTER PROGRAM
        {program}
    }}
}}
"""

pipeline_template = """
void pipeline(input_t* data_in, uint32_t reduction_idx) {{
    {temporaries}

    memcpy(&tmp_0, data_in, sizeof(tmp_0));

    {compute_stages}

// LOCAL REDUCTION
#ifdef SYNCHRONIZE_REDUCTION
    mutex_lock(&reduction_mutexes[reduction_idx]);
#endif

    pipeline_reduce(&reduction_vars[reduction_idx], &{compute_output});

#ifdef SYNCHRONIZE_REDUCTION
    mutex_unlock(&reduction_mutexes[reduction_idx]);
#endif
}}
"""

reduce_template = """
void pipeline_reduce(reduction_out_t* restrict out_ptr, const reduction_in_t* restrict in_ptr) {{
    reduction_in_t in;
    reduction_out_t out;
    memcpy(&in, in_ptr, sizeof(in));
    memcpy(&out, out_ptr, sizeof(out));
    {{
        {program}
    }}
    memcpy(out_ptr, &out, sizeof(out));
}}
"""

reduce_combine_template = """
void pipeline_reduce_combine(reduction_out_t* restrict out_ptr, const reduction_out_t* restrict in_ptr) {{
    reduction_out_t in;
    reduction_out_t out;
    memcpy(&in, in_ptr, sizeof(in));
    memcpy(&out, out_ptr, sizeof(out));
    {{
        {program}
    }}
    memcpy(out_ptr, &out, sizeof(out));
}}
"""

setup_inputs_template = """
void setup_inputs() {{
    // read aligned MRAM chunk containing global data
    __dma_aligned uint8_t buf[GLOBALS_SIZE_ALIGNED];
    mram_read(globals_input_buffer, buf, GLOBALS_SIZE_ALIGNED);

    memcpy(&total_input_elems, &buf[0], sizeof(total_input_elems));

    // initialize global variables
    {globals_init}
}}
"""

main_template = """
#include "common.h"

#include <barrier.h>
#include <defs.h>
#include <handshake.h>
#include <mram.h>
#include <mutex.h>
#include <seqread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if REDUCTION_VAR_COUNT < NR_TASKLETS
#define SYNCHRONIZE_REDUCTION
#elif REDUCTION_VAR_COUNT > NR_TASKLETS
#error Cannot have more reduction variables than tasklets
#endif

// host input globals
uint32_t total_input_elems;

{host_globals}

// constant globals

{const_globals}

// streaming data input
__mram_noinit uint8_t element_input_buffer[INPUT_BUF_SIZE];
__mram_noinit uint8_t globals_input_buffer[GLOBALS_SIZE_ALIGNED];

// data output
__host reduction_out_t reduction_output;

// reduction values and helpers
reduction_out_t reduction_vars[REDUCTION_VAR_COUNT];
__atomic_bit uint8_t reduction_mutexes[REDUCTION_VAR_COUNT];

// various barriers
BARRIER_INIT(setup_barrier, NR_TASKLETS);
BARRIER_INIT(reduction_init_barrier, NR_TASKLETS);
BARRIER_INIT(final_reduction_barrier, NR_TASKLETS);


void pipeline(input_t* data_in, uint32_t reduction_idx);
void setup_inputs();
void setup_reduction(uint32_t reduction_idx);
void reduce();

int main() {{
    uint32_t index = me();
    if (index == 0) {{
        setup_inputs();
    }}
    barrier_wait(&setup_barrier);

    size_t input_elem_count = total_input_elems / NR_TASKLETS;
    size_t remaining_elems = total_input_elems % NR_TASKLETS;

    // distribute N remaining elements onto first N tasklets
    if (index < remaining_elems) {{
        input_elem_count += 1;
    }}

    size_t local_offset = input_elem_count * index;
    if (index >= remaining_elems) {{
        local_offset += remaining_elems;
    }}

    size_t reduction_idx = index % REDUCTION_VAR_COUNT;

    // init local reduction variable
    if (index == reduction_idx) {{
        setup_reduction(reduction_idx);
    }}
    barrier_wait(&reduction_init_barrier);

    seqreader_buffer_t local_cache = seqread_alloc();
    seqreader_t sr;

    input_t* current_read = seqread_init(
        local_cache, &element_input_buffer[local_offset * sizeof(input_t)], &sr);

    for (size_t i = 0; i < input_elem_count; ++i) {{
        pipeline(current_read, reduction_idx);
        current_read = seqread_get(current_read, sizeof(input_t), &sr);
    }}

    // only main tasklet performs final reduction
    barrier_wait(&final_reduction_barrier);
    if (index == 0) {{
        reduce();
    }}

    if (index == 0) {{
        printf("ok\\n");
    }}
    return 0;
}}

// GENERATED CODE
"""

setup_reduction_template = """
void setup_reduction(uint32_t reduction_idx) {{
    memcpy(&reduction_vars[reduction_idx], &{identity}, sizeof({identity}));
}}
"""

host_main_template = """
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
                   {global_param_decl}
                   ) {{
    struct dpu_set_t dpu;
    uint32_t dpu_id;

    size_t base_inputs = elem_count / nr_dpus;
    size_t remaining_elems = elem_count % nr_dpus;

    DPU_FOREACH(set, dpu, dpu_id) {{
        size_t input_elem_count = base_inputs;
        if (dpu_id < remaining_elems) {{
            input_elem_count += 1;
        }}

        size_t local_offset = input_elem_count * dpu_id;
        if (dpu_id >= remaining_elems) {{
            local_offset += remaining_elems;
        }}

        DPU_ASSERT(dpu_prepare_xfer(dpu, (void*)&input[local_offset]));
    }}
    DPU_ASSERT(dpu_push_xfer(set, DPU_XFER_TO_DPU, "element_input_buffer", 0, sizeof(input_t) * (base_inputs + 1), DPU_XFER_DEFAULT));

    uint8_t globals_data_less[GLOBALS_SIZE_ALIGNED];
    memcpy(&globals_data_less[0], &base_inputs, sizeof(elem_count_t));

    {globals_init}

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

    DPU_FOREACH(set, dpu, dpu_id) {{
        if (dpu_id < remaining_elems) {{
            DPU_ASSERT(dpu_prepare_xfer(dpu, (void*)globals_data_more));
        }} else {{
            DPU_ASSERT(dpu_prepare_xfer(dpu, (void*)globals_data_less));
        }}
    }}
    DPU_ASSERT(dpu_push_xfer(set, DPU_XFER_TO_DPU, "globals_input_buffer", 0, GLOBALS_SIZE_ALIGNED, DPU_XFER_DEFAULT));
}}

void compute_final_result(struct dpu_set_t set, uint32_t nr_dpus, reduction_out_t* output) {{
    struct dpu_set_t dpu;
    uint32_t dpu_id;
    reduction_out_t outputs[nr_dpus];
    DPU_FOREACH(set, dpu, dpu_id) {{ DPU_ASSERT(dpu_prepare_xfer(dpu, &outputs[dpu_id])); }}
    DPU_ASSERT(
        dpu_push_xfer(set, DPU_XFER_FROM_DPU, "reduction_output", 0, sizeof(reduction_out_t), DPU_XFER_DEFAULT));


    for (int i = 1; i < nr_dpus; ++i) {{
        pipeline_reduce_combine(&outputs[0], &outputs[i]);
    }}

    memcpy(output, &outputs[0], sizeof(outputs[0]));
}}

int process(reduction_out_t* output, const input_t* input, size_t elem_count {global_param_decl}) {{
    struct dpu_set_t set, dpu;
    uint32_t nr_dpus;

    DPU_ASSERT(dpu_alloc(DPU_ALLOCATE_ALL, "backend=simulator", &set));
    DPU_ASSERT(dpu_load(set, DPU_BINARY, NULL));
    DPU_ASSERT(dpu_get_nr_dpus(set, &nr_dpus));

    setup_inputs(set, nr_dpus, input, elem_count {global_args});

    DPU_ASSERT(dpu_launch(set, DPU_SYNCHRONOUS));
    compute_final_result(set, nr_dpus, output);

    DPU_ASSERT(dpu_free(set));
    return 0;
}}
"""

host_header_template = """
#pragma once

#include "common.h"

int process(reduction_out_t* output, const input_t* input, size_t elem_count {global_param_decl});
"""

def create_map(index: int, program: str) -> str:
    name = f'stage_{index}'
    in_type = name + "_in_t"
    out_type = name + "_out_t"
    return map_template.format(func_name=name, in_type=in_type, out_type=out_type, program=program)

def create_filter(index: int, program: str) -> str:
    name = f'stage_{index}'
    in_type = name + "_in_t"
    return filter_template.format(func_name=name, in_type=in_type, program=program)

def create_reduce(program: str) -> str:
    return reduce_template.format(program=program)

def create_reduce_combine(program: str) -> str:
    return reduce_combine_template.format(program=program)

def create_map_call(index: int) -> str:
    return f"""
    stage_{index}(&tmp_{index}, &tmp_{index + 1});
    """

def create_filter_call(index: int) -> str:
    return f"""
    if (!stage_{index}(&tmp_{index})) {{
        return;
    }}
    memcpy(&tmp_{index + 1}, &tmp_{index}, sizeof(tmp_{index + 1}));
    """

def create_pipeline_temporary(index: int) -> str:
    return f'stage_{index}_in_t tmp_{index};\n'

def create_typedef(name: str, type: str) -> str:
    base, bracket, rest = type.partition('[')
    return f"typedef {base} {name}{bracket + rest};\n"

def create_pipeline(temporaries: str, compute_stages: str, stage_count: int) -> str:
    return pipeline_template.format(temporaries=temporaries, compute_stages=compute_stages, compute_output=f"tmp_{stage_count - 1}")

def create_global_reduce():
    return """
void reduce() {
    memcpy(&reduction_output, &reduction_vars[0], sizeof(reduction_vars[0]));
    for (size_t i = 1; i < REDUCTION_VAR_COUNT; ++i) {
        pipeline_reduce_combine(&reduction_output, &reduction_vars[i]);
    }
}
"""

def create_define(name: str, value) -> str:
    return f"#define {name} {value}\n"

def create_inputs_setup(globals_init: str) -> str:
    return setup_inputs_template.format(globals_init=globals_init)

def create_host_main(num_globals: int) -> str:
    global_param_decl = ""
    global_args = ""
    globals_init = ""
    for i in range(num_globals):
        global_param_decl += f", const global_{i}_t* global_{i}"
        global_args += f", global_{i}"
        globals_init += f"memcpy(&globals_data_less[GLOBAL_{i}_OFFSET], global_{i}, sizeof(global_{i}_t));\n"
    return host_main_template.format(global_param_decl=global_param_decl, global_args=global_args, globals_init=globals_init)

def create_host_header(num_globals: int) -> str:
    global_param_decl = ""
    for i in range(num_globals):
        global_param_decl += f", const global_{i}_t* global_{i}"
    return host_header_template.format(global_param_decl=global_param_decl)

import tomllib

with open('example_input.toml', 'rb') as f:
    data = tomllib.load(f)

    common_header = """
#pragma once

#include "stdint.h"
#include "stddef.h"

"""

    common_header += create_define("REDUCTION_VAR_COUNT", 12)
    common_header += create_define("INPUT_BUF_SIZE", "(1 << 16)")
    common_header += '\n'

    typedefs = ""


    # process global definitions
    general = data['pipeline']
    typedefs += create_typedef("input_t", general['input'])
    typedefs += create_typedef("elem_count_t", "uint32_t")

    globals_declarations = ""
    globals_init = ""
    offset_defines = ""

    if 'globals' not in general:
        general['globals'] = []

    for idx, global_value in enumerate(general['globals']):
        name = global_value['name']
        typedefs += create_typedef(f"global_{idx}_t", global_value['type'])
        globals_declarations += f"global_{idx}_t {name};\n"
        if idx == 0:
            offset_defines += create_define(f"GLOBAL_{idx}_OFFSET", "sizeof(elem_count_t)")
        else:
            offset_defines += create_define(f"GLOBAL_{idx}_OFFSET", f"(GLOBAL_{idx - 1}_OFFSET + sizeof(global_{idx - 1}_t))")
        globals_init += f"memcpy(&{name}, &buf[GLOBAL_{idx}_OFFSET], sizeof({name}));\n"

    num_globals = len(general['globals'])
    if num_globals == 0:
        offset_defines += create_define("GLOBALS_SIZE", "sizeof(elem_count_t)")
    else:
        offset_defines += create_define("GLOBALS_SIZE", f"(GLOBAL_{num_globals - 1}_OFFSET + sizeof(global_{num_globals - 1}_t))")
    offset_defines += create_define("GLOBALS_SIZE_ALIGNED", "(((GLOBALS_SIZE - 1) | 7) + 1)")


    constants_declarations = ""
    for idx, constant in enumerate(general['constants']):
        typedefs += create_typedef(f"constant_{idx}_t", constant['type'])
        constants_declarations += f"constant_{idx}_t {constant['name']} = {constant['init']};\n"

    device_code = main_template.format(host_globals=globals_declarations, const_globals=constants_declarations)
    device_code += create_inputs_setup(globals_init)

    host_code = create_host_main(num_globals)
    host_header = create_host_header(num_globals)

    pipeline_temporaries = ""
    pipeline_compute_stages = ""
    # process individual stages
    for idx, stage in enumerate(data['stages']):
        if idx == 0:
            typedefs += create_typedef("stage_0_in_t", "input_t")
        else:
            typedefs += create_typedef(f"stage_{idx}_in_t", f"stage_{idx - 1}_out_t")

        if 'output' in stage:
            typedefs += create_typedef(f"stage_{idx}_out_t", stage['output'])
        else:
            typedefs += create_typedef(f"stage_{idx}_out_t", f"stage_{idx}_in_t")

        pipeline_temporaries += create_pipeline_temporary(idx)
        kind = stage['kind']
        if kind == 'map':
            device_code += create_map(idx, stage['program'])
            pipeline_compute_stages += create_map_call(idx)
        elif kind == 'filter':
            device_code += create_filter(idx, stage['program'])
            pipeline_compute_stages += create_filter_call(idx)
        elif kind == 'reduce':
            device_code += create_reduce(stage['program'])
            device_code += create_reduce_combine(stage['combine'])
            device_code += setup_reduction_template.format(identity=stage['identity'])

            host_code += create_reduce_combine(stage['combine'])
            # generate additional typedef for reduce stage
            typedefs += create_typedef("reduction_in_t", f"stage_{idx}_in_t")
            typedefs += create_typedef("reduction_out_t", f"stage_{idx}_out_t")
    
    common_header += typedefs + '\n'
    common_header += offset_defines + '\n'

    device_code += create_pipeline(pipeline_temporaries, pipeline_compute_stages, len(data['stages']))
    device_code += create_global_reduce()

    with open('output/common.h', 'w') as out:
        out.write(common_header)

    with open('output/device.c', 'w') as out:
        out.write(device_code)

    with open('output/host.c', 'w') as out:
        out.write(host_code)

    with open('output/host.h', 'w') as out:
        out.write(host_header)
