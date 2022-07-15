#!/usr/bin/python3.10

import matplotlib.pyplot as plt
import numpy as np
import csv
import os

def create_plot(name: str, data: dict[str, list[float]]):
    if len(data) == 0:
        return

    wd = 1 / len(data)

    x_pos = np.arange(1, 2*len(list(data.values())[0]), 2)

    plt.figure()

    for i, (k, d) in enumerate(data.items()):
        plt.bar(x_pos + i * wd, d, width=wd, label=k, edgecolor='k')

    plt.xticks(x_pos+wd, ["cpu -> dpu", "dpu compute", "dpu -> cpu", "cpu merge", "total"], fontsize=10)
    plt.yticks(fontsize=10)
    plt.title(f"{name}", fontsize=15)
    plt.ylabel('us', fontsize=10)

    plt.legend(loc="upper center", fontsize=10)

    plt.savefig(f"results/plot_{name}.png", dpi=300)

def read_inputs(dir, base, someopt, fullopt, reference):
    inputs = {}

    def read_file(filename, category):
        with open(os.path.join(dir, filename), newline='') as file_data:
            reader = csv.reader(file_data)
            next(reader, None) # skip header
            for line in reader:
                name = line[0]
                data = list(map(float, line[1:]))

                if name not in inputs:
                    inputs[name] = {}

                inputs[name][category] = data

    read_file(base, "base")
    read_file(someopt, "someopt")
    read_file(fullopt, "fullopt")
    read_file(reference, "reference")

    return inputs


def main():
    out = read_inputs("results", "out_O0.csv", "out_O1.csv", "out_O2.csv", "out_ref.csv")
    for k, val in out.items():
        create_plot(k, val)

main()
