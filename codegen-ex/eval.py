#!/usr/bin/python3.10

import matplotlib.pyplot as plt
import numpy as np
import csv
import os
import json

def create_plot(name: str, data: dict[str, list[float]]):
    data1 = data["base"]
    data2 = data["someopt"]
    data3 = data["fullopt"]

    print(name)
    print(data1)
    print(data2)
    print(data3)

    wd = 0.3

    x_pos = np.arange(1, 2*len(data1), 2)

    plt.figure()

    # plt.semilogy()
    plt.bar(x_pos, data1, color='r', width=wd, label="O0", edgecolor='k')
    plt.bar(x_pos + wd, data2, color='y', width=wd, label="O1", edgecolor='k')
    plt.bar(x_pos + 2 * wd, data3, color='c', width=wd, label="O2", edgecolor='k')

    plt.xticks(x_pos+wd, ["cpu -> dpu", "dpu compute", "dpu -> cpu", "cpu merge", "total"], fontsize=10)
    plt.yticks(fontsize=10)
    plt.title(f"{name}", fontsize=15)
    plt.ylabel('us', fontsize=10)

    plt.legend(loc="upper center", fontsize=10)

    plt.savefig(f"results/plot_{name}.png", dpi=300)

def read_inputs(dir, base, someopt, fullopt, reference, cpu):
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
    # read_file(reference, "reference")
    # read_file(cpu, "cpu")

    return inputs


def main():
    out = read_inputs("results", "out_O0.csv", "out_O1.csv", "out_O2.csv", None, None)
    for k, val in out.items():
        create_plot(k, val)

main()
