#!/usr/bin/python3.10

import matplotlib.pyplot as plt
import numpy as np
import csv
import os

def create_plot(name: str, data: dict[str, list[list[float]]]):
    print("plotting:", name)
    if len(data) == 0:
        return

    fig = plt.figure(figsize=(9, 6))
    fig.subplots_adjust(wspace=0.25, hspace=0.25)
    ax = []
    ax.append(plt.subplot(2, 3, 1))
    ax.append(plt.subplot(2, 3, 2))
    ax.append(plt.subplot(2, 3, 4))
    ax.append(plt.subplot(2, 3, 5))
    ax.append(plt.subplot(2, 3, (3, 6)))

    # fig, ax = plt.subplots(1, 5, sharey=False, figsize=(16, 6)) # one figure for each of 5 steps

    plot_data = []

    for i, (kind, steps) in enumerate(data.items()):
        for j, step in enumerate(steps):
            plot = ax[j].violinplot(step, positions=[i], showmedians=True, showextrema=False)#, labels=[kind], widths=[wd])
            if j == 0:
                plot_data.append((plot, kind))
    
    ax[0].set_xlabel("cpu -> dpu", fontsize=12)
    ax[1].set_xlabel("dpu compute", fontsize=12)
    ax[2].set_xlabel("dpu -> cpu", fontsize=12)
    ax[3].set_xlabel("cpu merge", fontsize=12)
    ax[4].set_xlabel("total", fontsize=12)

    for axis in ax:
        axis.set_ylim(ymin=0)
        axis.ticklabel_format(style='sci', axis='y', scilimits=(0,0))
        axis.tick_params(
            axis='x',          # changes apply to the x-axis
            which='both',      # both major and minor ticks are affected
            bottom=False,      # ticks along the bottom edge are off
            top=False,         # ticks along the top edge are off
            labelbottom=False) # labels along the bottom edge are off

    # fig.xticks(x_pos+wd, ["cpu -> dpu", "dpu compute", "dpu -> cpu", "cpu merge", "total"], fontsize=10)
    # fig.yticks(fontsize=10)
    # fig.suptitle(f"{name}", fontsize=15)
    # fig.ylabel('us', fontsize=10)

    fig.legend([a["bodies"][0] for a, _ in plot_data], [b for _, b in plot_data], loc="lower center", fontsize=10, ncol=len(plot_data))

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
    out = read_inputs("results", "out_O0.csv", "out_O1.csv", "out_O2.csv", "out_ref.csv")
    for k, val in out.items():
        create_plot(k, val)

main()
