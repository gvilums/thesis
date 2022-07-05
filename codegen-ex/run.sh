#!/usr/bin/bash

function run_test {
    echo
    echo "testing " $1
    python3.11 ./pipeline.py $1 output
    gcc -D$(basename -s .toml $1 | tr '[:lower:]' '[:upper:]') -g -I/usr/include/dpu -I./output -L/usr/lib -ldpu ./output/host.c ./test/main.c -o ./build/host
    cp ./output/device ./build/device
    cd build
    ./host
    cd ..
}


for filename in ./inputs/*; do
    run_test $filename
done
