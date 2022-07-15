#!/usr/bin/bash
rm -f ref_out.csv
echo "name, cpu -> dpu, dpu compute, dpu -> cpu, combine, total" >> ./ref_out.csv

cd tests/histogram_large
make
./bin/host_code -i 100000000 >> ../../ref_out.csv
cd ../..

cd tests/histogram_small
make
./bin/host_code -i 100000000 >> ../../ref_out.csv
cd ../..

cd tests/select
make
./bin/host_code -i 100000000 >> ../../ref_out.csv
cd ../..
k
cd tests/sum_reduce
make
./bin/host_code -i 100000000 >> ../../ref_out.csv
cd ../..

cd tests/vector_add
make
./bin/host_code -i 100000000 >> ../../ref_out.csv
cd ../..
