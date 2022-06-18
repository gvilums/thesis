/usr/bin/python3.11 ./script/translate.py
cd build
rm -r *
dpu-upmem-dpurte-clang -o device -DNR_TASKLETS=16 -g -O ../output/device.c
gcc -o host -g -I/usr/local/upmem/include/dpu -L/usr/local/upmem/lib -ldpu ../output/host.c ../output/main.c
./host
cd ..