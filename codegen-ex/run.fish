cd build
rm -r *
dpu-upmem-dpurte-clang -o device -DNR_TASKLETS=16 -g ../src/device/*.c
g++ -o host -I/usr/local/upmem/include/dpu -L/usr/local/upmem/lib -ldpu ../src/host/*.cpp
./host
cd ..