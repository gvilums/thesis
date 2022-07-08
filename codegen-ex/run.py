#!/usr/bin/python3

import os, shutil, sys, subprocess

from os.path import join

def run_test(input_file, out_dir, build_dir):
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

    args += ["-g"]
    args += [sim_define, f"-D{test_name.upper()}"]
    args += [upmem_include, f"-I{out_dir}"]
    args += [upmem_link, "-ldpu", "-lpthread"] # -ltbb for std::execution
    args += [join(out_dir, "host.cpp"), "./test/main.cpp"]
    args += ["-o", join(build_dir, "host")]

    subprocess.run(args, check=True)

    os.chdir(build_dir)
    result = subprocess.run("./host", capture_output=True)
    if result.returncode != 0:
        print("ERROR while running program:")
        print(" ---- stderr ----")
        print(result.stderr.decode("utf8"))
        print(" ---- stdout ----")
        print(result.stdout.decode("utf8"))
    else:
        print(result.stdout.decode("utf8"))
    os.chdir("..")

    print()



def main():
    if len(sys.argv) == 1:
        for filename in os.listdir("inputs"):
            run_test(f"inputs/{filename}", "output", "build")
    elif len(sys.argv) == 2:
        run_test(f"inputs/{sys.argv[1]}.toml", "output", "build")
    else:
        print("usage: run.py [filename]")

main()