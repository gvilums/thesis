#!/usr/bin/python3.10

import matplotlib.pyplot as plt
import numpy as np
import csv
import os

def create_plot(name: str, data: dict[str, list[list[float]]]):
    if len(data) == 0:
        return

    wd = 1 / len(data)

    x_pos = np.arange(1, 2*len(list(data.values())[0]), 2)

    fig, ax = plt.subplots(1, 5, figsize=(20, 6)) # one figure for each of 5 steps

    

    for i, (kind, steps) in enumerate(data.items()):
        for j, step in enumerate(steps):
            ax[j].boxplot(step, positions=[i], labels=[kind], widths=[wd])
        # plt.boxplot(steps, positions=x_pos + i * wd, widths=[wd for _ in steps], labels=[kind for _ in steps])
    
    ax[0].set_title("cpu -> dpu")
    ax[1].set_title("dpu compute")
    ax[2].set_title("dpu -> cpu")
    ax[3].set_title("cpu merge")
    ax[4].set_title("total")

    # fig.xticks(x_pos+wd, ["cpu -> dpu", "dpu compute", "dpu -> cpu", "cpu merge", "total"], fontsize=10)
    # fig.yticks(fontsize=10)
    fig.suptitle(f"{name}", fontsize=15)
    # fig.ylabel('us', fontsize=10)

    fig.legend(loc="upper center", fontsize=10)

    fig.savefig(f"results/plot_{name}.svg")

def read_inputs(dir, base, someopt, fullopt, reference):
    inputs = {}

    def read_file(filename, category):
        with open(os.path.join(dir, filename), newline='') as file_data:
            reader = csv.reader(file_data)
            steps = []
            for line in reader:
                name = line[0]
                step_idx = int(line[1])
                if step_idx == 0:
                    steps = []

                data = list(map(float, line[2:-1]))
                steps.append(data)

                if name not in inputs:
                    inputs[name] = {}

                if step_idx == 4:
                    inputs[name][category] = steps

    read_file(base, "base")
    read_file(someopt, "someopt")
    read_file(fullopt, "fullopt")
    read_file(reference, "reference")

    return inputs


def main():
    out = read_inputs("results", "out_O0.csv", "out_O0.csv", "out_O0.csv", "out_O0.csv")
    for k, val in out.items():
        create_plot(k, val)

main()
