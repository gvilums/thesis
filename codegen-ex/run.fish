#!/usr/bin/fish

function run_test
    /usr/bin/python3.11 ./pipeline.py $argv[1] output
    dpu-upmem-dpurte-clang -DSTACK_SIZE_DEFAULT=1024 -DNR_TASKLETS=16 -g -O ./output/device.c -o ./build/device
    gcc -D(string upper (basename -s .toml $argv[1])) -g -I/usr/local/upmem/include/dpu -L/usr/local/upmem/lib -ldpu ./output/host.c ./output/main.c -o ./build/host
    cd build
    ./host
    cd ..
end


for filename in ./inputs/*
    run_test $filename &
end

wait