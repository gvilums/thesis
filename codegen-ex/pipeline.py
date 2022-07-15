from mako.template import Template
from mako.lookup import TemplateLookup
from enum import Enum
import tomli
import sys
import dataclasses
import subprocess
import re
import tempfile
import math
import os

if os.path.exists("/usr/local/upmem/bin"):
    dpu_compiler = "/usr/local/upmem/bin/dpu-upmem-dpurte-clang"
    dpu_stack_analyzer = "/usr/local/upmem/bin/dpu_stack_analyzer"
else:
    dpu_compiler = "dpu-upmem-dpurte-clang"
    dpu_stack_analyzer = "dpu_stack_analyzer"

def main():
    if len(sys.argv) != 4:
        print(f"usage: {sys.argv[0]} <configuration file> <output directory> <opt level>")
        return

    config_name = sys.argv[1]
    output_dir = sys.argv[2]
    opt_level = int(sys.argv[3])
    lookup = TemplateLookup(directories=["templates/base", "templates/reduce", "templates/noreduce", "templates/util"])

    config = read_config(config_name)

    normalize_config(config)
    config["pipeline"]["nr_tasklets"] = 11

    if config["stages"][-1]["kind"] == "reduce":
        generator = create_reduce_pipeline
    else:
        generator = create_noreduce_pipeline

    base_code = generator(config, lookup)

    size_info = compute_size_info(config, lookup, base_code)
    if config["stages"][-1]["kind"] == "reduce":
        params = optimize_reduce_params(size_info, opt_level)
    else:
        params = optimize_noreduce_params(size_info, opt_level)

    # print(params)
    # print(size_info)

    params.apply_to(config)

    final_code = generator(config, lookup)
    final_code.output_to(output_dir)
    subprocess.run([dpu_compiler, f"-DSTACK_SIZE_DEFAULT={size_info.stack_size}",
                   f"-DNR_TASKLETS={params.nr_tasklets}", "-g", "-O2", f"{output_dir}/device.c", "-o", f"{output_dir}/device"], check=True)


@dataclasses.dataclass
class CodegenOutput:
    device_code: str
    common_header: str
    host_code: str
    host_header: str

    def output_to(self, output_dir: str):
        with open(f'{output_dir}/common.h', 'w') as out:
            out.write(self.common_header)

        with open(f'{output_dir}/device.c', 'w') as out:
            out.write(self.device_code)

        with open(f'{output_dir}/host.cpp', 'w') as out:
            out.write(self.host_code)

        with open(f'{output_dir}/host.hpp', 'w') as out:
            out.write(self.host_header)


@dataclasses.dataclass
class SizeInformation:
    stack_size: int
    global_sizes: list[int]
    constant_sizes: list[int]
    input_sizes: list[int]
    output_size: int


@dataclasses.dataclass
class ReduceConfigParams:
    nr_tasklets: int
    nr_reduction_vars: int
    read_cache_size: int

    def apply_to(self, config):
        config["pipeline"]["nr_tasklets"] = self.nr_tasklets
        config["pipeline"]["reduction_vars"] = self.nr_reduction_vars
        config["pipeline"]["read_cache_size"] = self.read_cache_size


@dataclasses.dataclass
class NoreduceConfigParams:
    nr_tasklets: int
    read_cache_size: int
    write_cache_size: int

    def apply_to(self, config):
        config["pipeline"]["nr_tasklets"] = self.nr_tasklets
        config["pipeline"]["read_cache_size"] = self.read_cache_size
        config["pipeline"]["write_cache_size"] = self.write_cache_size


def read_config(filename: str):
    with open(filename, "rb") as f:
        return tomli.load(f)


def compute_input_indices(stages):
    stages[0]["input_idx"] = -1
    last_stable = -1
    for prev_idx, stage in enumerate(stages[1:]):
        if stages[prev_idx]["kind"] != "filter":
            last_stable = prev_idx
        stage["input_idx"] = last_stable


def index_stages(stages):
    for idx, stage in enumerate(stages):
        stage["id"] = idx


def compute_filter_info(config):
    contains_filter = False
    for stage in config["stages"]:
        if stage["kind"] == "filter":
            contains_filter = True
            break
    if not contains_filter:
        config["pipeline"]["no_filter"] = True
    else:
        for stage in reversed(config["stages"]):
            if stage["kind"] == "filter":
                stage["last_filter"] = True
                break


def create_reduce_pipeline(config, lookup: TemplateLookup) -> CodegenOutput:

    device_code_template = lookup.get_template("device_reduce.c")
    common_header_template = lookup.get_template("common_reduce.h")
    host_code_template = lookup.get_template("host_reduce.cpp")
    host_header_template = lookup.get_template("host_reduce.hpp")

    pipeline = config["pipeline"]
    in_stage = config["stages"][0]
    stages = config["stages"][1:len(config["stages"]) - 1]
    reduction = config["stages"][-1]

    device_code = device_code_template.render(pipeline=pipeline, stages=stages, reduction=reduction, in_stage=in_stage)
    common_header = common_header_template.render(pipeline=pipeline, stages=stages, reduction=reduction, in_stage=in_stage)
    host_code = host_code_template.render(pipeline=pipeline, stages=stages, reduction=reduction, in_stage=in_stage)
    host_header = host_header_template.render(pipeline=pipeline, stages=stages, reduction=reduction, in_stage=in_stage)

    return CodegenOutput(device_code, common_header, host_code, host_header)


def create_noreduce_pipeline(config, lookup: TemplateLookup) -> CodegenOutput:
    device_code_template = lookup.get_template("device_noreduce.c")
    common_header_template = lookup.get_template("common_noreduce.h")
    host_code_template = lookup.get_template("host_noreduce.cpp")
    host_header_template = lookup.get_template("host_noreduce.hpp")

    pipeline = config["pipeline"]
    in_stage = config["stages"][0]
    stages = config["stages"][1:len(config["stages"]) - 1]
    output = config["stages"][-1]

    device_code = device_code_template.render(pipeline=pipeline, stages=stages, output=output, in_stage=in_stage)
    common_header = common_header_template.render(pipeline=pipeline, stages=stages, output=output, in_stage=in_stage)
    host_code = host_code_template.render(pipeline=pipeline, stages=stages, output=output, in_stage=in_stage)
    host_header = host_header_template.render(pipeline=pipeline, stages=stages, output=output, in_stage=in_stage)

    return CodegenOutput(device_code, common_header, host_code, host_header)


def normalize_config(config):
    assert "pipeline" in config
    assert len(config["stages"]) > 0

    has_reduce = False

    assert "stages" in config
    for i, stage in enumerate(config["stages"]):
        assert "kind" in stage
        if i == 0:
            assert stage["kind"] == "input"
            assert "inputs" in stage
            assert len(stage["inputs"]) >= 1
            if "program" not in stage:
                assert len(stage["inputs"]) == 1
                stage["program"] = "memcpy(out_ptr, in_ptr_0, sizeof(*out_ptr));\n"
            if "output" not in stage:
                assert len(stage["inputs"]) == 1
                stage["output"] = stage["inputs"][0]
        else:
            assert stage["kind"] in ["map", "filter", "reduce"]
        if stage["kind"] == "reduce":
            has_reduce = True
            assert i == len(config["stages"]) - 1

    if not has_reduce:
        config["stages"].append({"kind": "output"})

    if "globals" not in config["pipeline"]:
        config["pipeline"]["globals"] = []

    if "constants" not in config["pipeline"]:
        config["pipeline"]["constants"] = []

    # default parameters for initial codegen
    config["pipeline"]["nr_tasklets"] = 11
    # assume two reduction variables:
    # - can't use optimistic estimate, as that may produce linker errors
    #   for size check
    # - can't use one, as that excludes reduce_combine from stack analysis
    config["pipeline"]["reduction_vars"] = 2 
    config["pipeline"]["read_cache_size"] = 512
    config["pipeline"]["write_cache_size"] = 512

    # various indexing operations required for codegen
    compute_input_indices(config["stages"])
    index_stages(config["stages"])
    compute_filter_info(config)


def compute_size_info(config, lookup: TemplateLookup, code: CodegenOutput) -> SizeInformation:
    size_info_template = lookup.get_template("size_info.c")
    num_inputs = len(config["stages"][0]["inputs"])
    num_globals = len(config["pipeline"]["globals"])
    num_constants = len(config["pipeline"]["constants"])
    with tempfile.TemporaryDirectory() as tmpdir:
        code.output_to(tmpdir)
        with open(f"{tmpdir}/size_info.c", "w") as f:
            f.write(size_info_template.render(num_inputs=num_inputs,
                    num_globals=num_globals, num_constants=num_constants))

        subprocess.run([dpu_compiler, "-DSTACK_SIZE_DEFAULT=256", "-DNR_TASKLETS=16",
                       "-g", "-O2", f"{tmpdir}/device.c", "-o", f"{tmpdir}/device"], check=True)
        stack_analysis_output = subprocess.run(
            [dpu_stack_analyzer, f"{tmpdir}/device"], capture_output=True).stdout.decode("utf8")
        stack_size_re_res = re.search(r"Max size: (\d+)\n", stack_analysis_output)
        # align stack size to 8 bytes
        stack_size = ((int(stack_size_re_res.group(1)) - 1) | 7) + 1

        subprocess.run(["cc", f"{tmpdir}/size_info.c", "-o", f"{tmpdir}/size_info"], check=True)
        size_info_output = subprocess.run([f"{tmpdir}/size_info"], capture_output=True).stdout.decode("utf8").split()
        assert len(size_info_output) == num_globals + num_constants + num_inputs + 1
        global_sizes = list(map(int, size_info_output[0:num_globals]))
        constant_sizes = list(map(int, size_info_output[num_globals:(num_globals + num_constants)]))
        input_sizes = list(map(int, size_info_output[(num_globals + num_constants):-1]))
        output_size = int(size_info_output[-1])

        return SizeInformation(stack_size, global_sizes, constant_sizes, input_sizes, output_size)


def optimize_reduce_params(sizes: SizeInformation, opt_level: int) -> ReduceConfigParams:
    if max(sizes.input_sizes) > 1024:
        raise "input element too large"

    num_inputs = len(sizes.input_sizes)

    base_size = pow(2, 16)  # default WRAM size is 64KB == 2^16 B
    base_size -= sum(sizes.global_sizes) + sum(sizes.constant_sizes) + 2048

    if 0 <= opt_level <= 1:
        try:

            input_buf_size = max(
                pow(2, math.ceil(math.log2(max(sizes.input_sizes)))), 256)
            reduction_var_count = min((base_size - 11 * (sizes.stack_size +
                                input_buf_size * num_inputs)) // sizes.output_size, 11)

            if opt_level == 0:
                # naive approach: if we can't fit all reduction variables, just use a single one
                if 0 < reduction_var_count < 11:
                    reduction_var_count = 1

            if reduction_var_count >= 1:
                return ReduceConfigParams(11, reduction_var_count, input_buf_size)

        except:
            pass

        raise "unable to configure program parameters"

    # This is the maximum optimization case

    try:

        input_buf_size = (base_size / 11 - sizes.stack_size -
                        sizes.output_size) / num_inputs
        input_buf_size = min(pow(2, math.floor(math.log2(input_buf_size))), 1024)

        # check if valid
        if input_buf_size >= 32 and all(map(lambda x: x <= input_buf_size, sizes.input_sizes)):
            return ReduceConfigParams(11, 11, input_buf_size)

    except:
        pass

    # decide between (T == 11 and R < T) or (T == R and T < 11)
    # this choice should be based on the relative size difference between reduction variables
    # and minimum input buffer sizes

    # for now, assume that we want to stay at T == 11. Reduce R instead

    # reasonable choice for input buf size: enough to fit any input, but not smaller than 256
    try:

        input_buf_size = max(
            pow(2, math.ceil(math.log2(max(sizes.input_sizes)))), 256)
        reduction_var_count = (base_size - 11 * (sizes.stack_size +
                            input_buf_size * num_inputs)) // sizes.output_size

        if reduction_var_count >= 1:
            return ReduceConfigParams(11, reduction_var_count, input_buf_size)

    except:
        pass

    raise "unable to configure program parameters"


def optimize_noreduce_params(sizes: SizeInformation, opt_level: int) -> NoreduceConfigParams:
    if max(sizes.input_sizes) > 1024:
        raise "input element too large"

    num_inputs = len(sizes.input_sizes)

    base_size = pow(2, 16)  # default WRAM size is 64KB == 2^16 B
    base_size -= sum(sizes.global_sizes) + sum(sizes.constant_sizes) + 2048

    # low or no optimization (equivalent in this case)
    if 0 <= opt_level <= 1:
        try:

            buf_size = max(
                pow(2, math.ceil(math.log2(max(sizes.input_sizes)))), 256)

            if buf_size >= 32 and all(map(lambda x: x <= buf_size, sizes.input_sizes + [sizes.output_size])):
                return NoreduceConfigParams(11, buf_size, buf_size)
        
        except:
            pass

        # TODO: in case the buffers don't fit, try reducing number of tasklets

        raise "unable to configure program parameters"

    # full optimization

    try:

        buf_size = (base_size / 11 - sizes.stack_size) / (num_inputs + 1)
        buf_size = min(pow(2, math.floor(math.log2(buf_size))), 1024)

        if buf_size >= 32 and all(map(lambda x: x <= buf_size, sizes.input_sizes + [sizes.output_size])):
            return NoreduceConfigParams(11, buf_size, buf_size)
    
    except:
        pass

    # TODO: in case the buffers don't fit, try reducing number of tasklets

    raise "unable to configure program parameters"


if __name__ == "__main__":
    main()
