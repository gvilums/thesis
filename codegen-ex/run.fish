cd build
rm -r *
dpu-upmem-dpurte-clang -o device -DNR_TASKLETS=16 -g -O2 ../src/device/device_main.c
gcc -o host -g -I/usr/local/upmem/include/dpu -L/usr/local/upmem/lib -ldpu ../src/host/host.c
./host
cd ..