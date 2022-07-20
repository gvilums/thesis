#!/usr/bin/bash
echo "name, cpu -> dpu, dpu compute, dpu -> cpu, combine, total" > ./results/out_ref.csv

echo "running reference"
cd ref/histogram_large
make
./bin/host_code -i 100000000 >> ../../results/out_ref.csv
cd ../..

cd ref/histogram_small
make
./bin/host_code -i 100000000 >> ../../results/out_ref.csv
cd ../..

cd ref/select
make
./bin/host_code -i 100000000 >> ../../results/out_ref.csv
cd ../..
k
cd ref/sum_reduce
make
./bin/host_code -i 100000000 >> ../../results/out_ref.csv
cd ../..

cd ref/vector_add
make
./bin/host_code -i 100000000 >> ../../results/out_ref.csv
cd ../..

echo "running base"
./run.py -s5 -O0 > results/out_O0.csv
echo "running someopt"
./run.py -s5 -O1 > results/out_O1.csv
echo "running fullopt"
./run.py -s5 -O2 > results/out_O2.csv
