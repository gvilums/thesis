#!/usr/bin/python3

import os, shutil, sys, subprocess, argparse

from os.path import join

def run_test(input_file, out_dir, build_dir, size_multiplier: int):
    test_name = os.path.basename(input_file).split(".")[0]
    print(f"testing {test_name}")

    subprocess.run(["python3.11", "pipeline.py", input_file, out_dir], check=True)
    shutil.copy(join(out_dir, "device"), join(build_dir, "device"))

    if os.path.exists("/usr/local/upmem"):
        upmem_include = "-I/usr/local/upmem/include/dpu"
        upmem_link = "-L/usr/local/upmem/lib"
        sim_define = "-DSIMULATOR"
    else:
        upmem_include = "-I/usr/include/dpu"
        upmem_link = "-L/usr/lib"
        sim_define = "-DNO_SIMULATOR"
    
    args = ["c++"]

    args += ["-g", "-O3"]
    args += [sim_define, f"-D{test_name.upper()}", f"-DSIZE_FACTOR={size_multiplier}"]
    args += [upmem_include, f"-I{out_dir}"]
    args += [upmem_link, "-ldpu", "-lpthread"] # -ltbb for std::execution
    args += [join(out_dir, "host.cpp"), "./test/main.cpp"]
    args += ["-o", join(build_dir, "host")]

    compile_result = subprocess.run(args, check=True)
    if compile_result.returncode != 0:
        print(compile_result.stderr.decode("utf8"))
        compile_result.check_returncode()

    os.chdir(build_dir)
    result = subprocess.run("./host", stderr=subprocess.DEVNULL)
    if result.returncode != 0:
        print("ERROR while running program:")
    os.chdir("..")

    print()



def main():
    parser = argparse.ArgumentParser(description="Run map-reduce tests")
    parser.add_argument('input_files', metavar='file', type=str, nargs='*',
                    help='an input file to process')
    parser.add_argument('-s', '--size', dest='size_mult', type=int, action='store', default=10,
                        help='the multiplier for input sizes')

    args = parser.parse_args()
    if len(args.input_files) == 0:
        args.input_files += os.listdir("inputs")

    for filename in args.input_files:
        run_test(f"inputs/{filename}", "output", "build", args.size_mult)

try:
    main()
except KeyboardInterrupt:
    print()
    exit(130)
