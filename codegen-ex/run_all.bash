#!/usr/bin/bash
rm -f results/ref_out.csv
echo "name, cpu -> dpu, dpu compute, dpu -> cpu, combine, total" >> ./results/out_ref.csv

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
