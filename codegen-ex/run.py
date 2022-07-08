#!/usr/bin/python3

import os, shutil

from subprocess import call
from os.path import join

def run_test(input_file, out_dir, build_dir):
    test_name = os.path.basename(input_file).split(".")[0]
    print(f"testing {test_name}")

    call(["python3.11", "pipeline.py", input_file, out_dir])
    shutil.copy(join(out_dir, "device"), join(build_dir, "device"))

    if os.path.exists("/usr/local/upmem"):
        upmem_include = "-I/usr/local/upmem/include/dpu"
        upmem_link = "-L/usr/local/upmem/lib"
        sim_define = "-DSIMULATOR"
    else:
        upmem_include = "-I/usr/include/dpu"
        upmem_link = "-L/usr/lib"
        sim_define = "-DNO_SIMULATOR"

    call(["g++-12", "-g", "-c", join(out_dir, "copy_parallel.cpp"), "-o", join(build_dir, "copy_parallel.o")])
    call(["gcc-12", "-g", "-c", sim_define, upmem_include, join(out_dir, "host.c"), "-o", join(build_dir, "host.o")])
    call(["gcc-12", "-g", "-c", f"-D{test_name.upper()}", f"-I{out_dir}", "./test/main.c", "-o", join(build_dir, "main.o")])

    os.chdir(build_dir)

    call(["g++-12", "copy_parallel.o", "host.o", "main.o", upmem_link, "-ltbb", "-ldpu", "-o", "host"])
    call("./host")

    os.chdir("..")

    print()


for filename in os.listdir("inputs"):
    run_test(f"inputs/{filename}", "output", "build")
