#include <stdio.h>
#include <barrier.h>
#include <defs.h>

BARRIER_INIT(test, NR_TASKLETS);

int main() {
    if (me() == 0) {
        printf("%u\n", sizeof(test));
    }
}