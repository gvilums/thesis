from mako.template import Template
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


def main():
    data = read_config("example_input.toml")
    device_code_template = Template(filename="templates/device.c")
    common_header_template = Template(filename="templates/common.h")
    host_code_template = Template(filename="templates/host.c")
    host_header_template = Template(filename="templates/host.h")


    if "globals" not in data["pipeline"]:
        data["pipeline"]["globals"] = []

    if "constants" not in data["pipeline"]:
        data["pipeline"]["constants"] = []


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


if __name__ == "__main__":
    main()