from mako.template import Template
from mako.lookup import TemplateLookup
import tomllib
import sys
import dataclasses

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

        with open(f'{output_dir}/host.c', 'w') as out:
            out.write(self.host_code)

        with open(f'{output_dir}/host.h', 'w') as out:
            out.write(self.host_header)


def read_config(filename: str):
    with open(filename, "rb") as f:
        return tomllib.load(f)

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

def create_reduce_pipeline(data, lookup: TemplateLookup) -> CodegenOutput:

    device_code_template = lookup.get_template("device_reduce.c")
    common_header_template = lookup.get_template("common_reduce.h")
    host_code_template = lookup.get_template("host_reduce.c")
    host_header_template = lookup.get_template("host_reduce.h")

    compute_input_indices(data["stages"])
    index_stages(data["stages"])

    pipeline = data["pipeline"]
    in_stage = data["stages"][0]
    stages = data["stages"][1:len(data["stages"]) - 1]
    reduction = data["stages"][-1]

    pipeline["reduction_vars"] = 16

    device_code = device_code_template.render(pipeline=pipeline, stages=stages, reduction=reduction, in_stage=in_stage)
    common_header = common_header_template.render(pipeline=pipeline, stages=stages, reduction=reduction, in_stage=in_stage)
    host_code = host_code_template.render(pipeline=pipeline, stages=stages, reduction=reduction, in_stage=in_stage)
    host_header = host_header_template.render(pipeline=pipeline, stages=stages, reduction=reduction, in_stage=in_stage)

    return CodegenOutput(device_code, common_header, host_code, host_header)

def create_noreduce_pipeline(data, lookup: TemplateLookup) -> CodegenOutput:
    device_code_template = lookup.get_template("device_noreduce.c")
    common_header_template = lookup.get_template("common_noreduce.h")
    host_code_template = lookup.get_template("host_noreduce.c")
    host_header_template = lookup.get_template("host_noreduce.h")

    data["stages"].append({"kind": "output"})
    compute_input_indices(data["stages"])
    index_stages(data["stages"])
    compute_filter_info(data)

    pipeline = data["pipeline"]
    in_stage = data["stages"][0]
    stages = data["stages"][1:len(data["stages"]) - 1]
    output = data["stages"][-1]

    device_code = device_code_template.render(pipeline=pipeline, stages=stages, output=output, in_stage=in_stage)
    common_header = common_header_template.render(pipeline=pipeline, stages=stages, output=output, in_stage=in_stage)
    host_code = host_code_template.render(pipeline=pipeline, stages=stages, output=output, in_stage=in_stage)
    host_header = host_header_template.render(pipeline=pipeline, stages=stages, output=output, in_stage=in_stage)

    return CodegenOutput(device_code, common_header, host_code, host_header)

def normalize_config(config):
    assert "pipeline" in config
    assert len(config["stages"]) > 0

    assert "stages" in config
    for i, stage in enumerate(config["stages"]):
        assert "kind" in stage
        if i == 0:
            assert stage["kind"] == "input"
            assert "inputs" in stage
            if "program" not in stage:
                assert len(stage["inputs"]) == 1
                stage["program"] = "memcpy(out_ptr, in_ptr_0, sizeof(*out_ptr));\n"
            if "output" not in stage:
                assert len(stage["inputs"]) == 1
                stage["output"] = stage["inputs"][0]
        else:
            assert stage["kind"] in ["map", "filter", "reduce"]
        if stage["kind"] == "reduce":
            assert i == len(config["stages"]) - 1

    if "globals" not in config["pipeline"]:
        config["pipeline"]["globals"] = []

    if "constants" not in config["pipeline"]:
        config["pipeline"]["constants"] = []
    


def main():
    if len(sys.argv) != 3:
        print(f"usage: {sys.argv[0]} <configuration file> <output directory>")
        return
    config_name = sys.argv[1]
    output_dir = sys.argv[2]
    lookup = TemplateLookup(directories=["templates/base", "templates/reduce", "templates/noreduce"])

    config = read_config(config_name)

    normalize_config(config)
    config["pipeline"]["nr_tasklets"] = 16

    if config["stages"][-1]["kind"] == "reduce":
        code = create_reduce_pipeline(config, lookup)
    else:
        code = create_noreduce_pipeline(config, lookup)
    
    code.output_to(output_dir)


if __name__ == "__main__":
    main()
