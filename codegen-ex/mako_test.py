from mako.template import Template
from mako.lookup import TemplateLookup
import tomllib
import dataclasses

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

def create_reduce_pipeline(data, lookup: TemplateLookup):

    device_code_template = lookup.get_template("device.c")
    common_header_template = lookup.get_template("common.h")
    host_code_template = lookup.get_template("host.c")
    host_header_template = lookup.get_template("host.h")


    compute_input_indices(data["stages"])

    pipeline = data["pipeline"]
    stages = data["stages"][:len(data["stages"]) - 1]
    reduction = data["stages"][-1]

    pipeline["reduction_vars"] = 16

    device_code = device_code_template.render(pipeline=pipeline, stages=stages, reduction=reduction)
    common_header = common_header_template.render(pipeline=pipeline, stages=stages, reduction=reduction)
    host_code = host_code_template.render(pipeline=pipeline, stages=stages, reduction=reduction)
    host_header = host_header_template.render(pipeline=pipeline, stages=stages, reduction=reduction)

    with open('output/common.h', 'w') as out:
        out.write(common_header)

    with open('output/device.c', 'w') as out:
        out.write(device_code)

    with open('output/host.c', 'w') as out:
        out.write(host_code)

    with open('output/host.h', 'w') as out:
        out.write(host_header)


def create_noreduce_pipeline(data, lookup: TemplateLookup):
    device_code_template = lookup.get_template("device_noreduce.c")
    common_header_template = lookup.get_template("common_noreduce.h")
    host_code_template = lookup.get_template("host_noreduce.c")
    host_header_template = lookup.get_template("host_noreduce.h")

    data["stages"].append({"kind": "output"})
    compute_input_indices(data["stages"])

    pipeline = data["pipeline"]
    stages = data["stages"][:len(data["stages"]) - 1]

    device_code = device_code_template.render(pipeline=pipeline, stages=stages)
    common_header = common_header_template.render(pipeline=pipeline, stages=stages)
    host_code = host_code_template.render(pipeline=pipeline, stages=stages)
    host_header = host_header_template.render(pipeline=pipeline, stages=stages)

    with open('output/common.h', 'w') as out:
        out.write(common_header)

    with open('output/device.c', 'w') as out:
        out.write(device_code)

    with open('output/host.c', 'w') as out:
        out.write(host_code)

    with open('output/host.h', 'w') as out:
        out.write(host_header)


def main():
    lookup = TemplateLookup(directories=["templates"])
    data = read_config("inputs/filter_even.toml")
    if "globals" not in data["pipeline"]:
        data["pipeline"]["globals"] = []

    if "constants" not in data["pipeline"]:
        data["pipeline"]["constants"] = []

    if data["stages"][-1]["kind"] == "reduce":
        create_reduce_pipeline(data, lookup)
    else:
        create_noreduce_pipeline(data, lookup)


if __name__ == "__main__":
    main()