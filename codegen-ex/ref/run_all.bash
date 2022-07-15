#!/usr/bin/bash
rm -f ref_out.csv
echo "name, cpu -> dpu, dpu compute, dpu -> cpu, combine, total" >> ./ref_out.csv
for dir in ./tests/*; do
    cd $dir
    make
    ./bin/host_code >> ../../ref_out.csv
    cd ../..
done
