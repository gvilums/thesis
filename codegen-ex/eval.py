import matplotlib.pyplot as plt
import numpy as np
import csv
import os
import json

def create_plot():
    data1 = [23,85, 72, 43, 52]
    data2 = [42, 35, 21, 16, 9]
    width = 0.2

    fig, plots = plt.subplots(2, 5)
    plt.bar(np.arange(len(data1)), data1, width=width)
    plt.bar(np.arange(len(data2)) + width, data2, width=width)
    plt.savefig('data.png')

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
    # read_file(someopt, "someopt")
    # read_file(fullopt, "fullopt")
    # read_file(reference, "reference")
    # read_file(cpu, "cpu")

    return inputs


def main():
    out = read_inputs("results", "example.txt", None, None, None, None)
    for k, val in out.items():
        print(k, val)

main()