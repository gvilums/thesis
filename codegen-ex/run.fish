#!/usr/bin/fish

function run_test
    echo
    echo "testing " $argv[1]
    /usr/bin/python3.11 ./pipeline.py $argv[1] output
    g++-12 -g -c ./output/copy_parallel.cpp -o ./build/copy_parallel.o
    gcc-12 -g -c -DSIMULATOR -I/usr/local/upmem/include/dpu ./output/host.c -o ./build/host.o
    gcc-12 -g -c -D(string upper (basename -s .toml $argv[1])) -I./output ./test/main.c -o ./build/main.o
    cp ./output/device ./build/device
    cd build
    g++-12 copy_parallel.o host.o main.o -L/usr/local/upmem/lib -ltbb -ldpu -o host
    ./host 2> /dev/null
    cd ..
end


for filename in ./inputs/*
    run_test $filename
end

wait