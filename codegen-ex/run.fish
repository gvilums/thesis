#!/usr/bin/fish

function run_test
    echo
    echo "testing " $argv[1]
    /usr/bin/python3.11 ./pipeline.py $argv[1] output
    gcc -DSIMULATOR -D(string upper (basename -s .toml $argv[1])) -g -I/usr/local/upmem/include/dpu -I./output -L/usr/local/upmem/lib -ldpu ./output/host.c ./test/main.c -o ./build/host
    cp ./output/device ./build/device
    cd build
    ./host 2> /dev/null
    cd ..
end


for filename in ./inputs/*
    run_test $filename
end

wait