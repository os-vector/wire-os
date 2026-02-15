#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/time.h>

#include "pagingtest.h"

int mmap_test(int test_runs, unsigned long long alloc_size) {
    void *buf;
    int ret = -1;
    int rc;
    int i;
    struct timeval begin_time, end_time, elapsed_time;
    struct timeval total_time_mmap, total_time_munmap, total_time_in, total_time_out;

    timerclear(&total_time_mmap);
    timerclear(&total_time_munmap);
    timerclear(&total_time_in);
    timerclear(&total_time_out);

    for (i = 0; i < test_runs; i++) {
        gettimeofday(&begin_time, NULL);
        buf = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        gettimeofday(&end_time, NULL);
        if (buf == ((void *)-1)) {
            fprintf(stderr, "Failed to mmap anonymous memory: %s\n", strerror(errno));
            goto err_map;
        }
        timersub(&end_time, &begin_time, &elapsed_time);
        timeradd(&total_time_mmap, &elapsed_time, &total_time_mmap);

        gettimeofday(&begin_time, NULL);
        munmap(buf, alloc_size);
        gettimeofday(&end_time, NULL);
        timersub(&end_time, &begin_time, &elapsed_time);
        timeradd(&total_time_mmap, &elapsed_time, &total_time_mmap);
    }

    printf("mmap: %llu us\n", total_time_mmap.tv_sec * USEC_PER_SEC + total_time_mmap.tv_usec);
    printf("munmap: %llu us\n", total_time_munmap.tv_sec * USEC_PER_SEC + total_time_munmap.tv_usec);

    ret = 0;
    goto end;
err:
    munmap(buf, alloc_size);
end:
err_map:
    return ret;
}
