/usr/bin/python3.11 ./mako_test.py
cd build
rm -r *
dpu-upmem-dpurte-clang -DSTACK_SIZE_DEFAULT=1024 -DNR_TASKLETS=16 -g -O ../output/device.c -o device
gcc -g -I/usr/local/upmem/include/dpu -L/usr/local/upmem/lib -ldpu ../output/host.c ../output/main.c -o host
./host
cd ..