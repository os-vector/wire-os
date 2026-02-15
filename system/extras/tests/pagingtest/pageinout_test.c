#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/time.h>

#include "pagingtest.h"

int pageinout_test(int test_runs, unsigned long long file_size) {
    int fd;
    char tmpname[] = "pageinoutXXXXXX";
    unsigned char *vec;
    int i;
    long long j;
    volatile char *buf;
    int ret = -1;
    int rc;
    struct timeval begin_time, end_time, elapsed_time, total_time_in, total_time_out;
    long pagesize = sysconf(_SC_PAGE_SIZE);

    timerclear(&total_time_in);
    timerclear(&total_time_out);

    fd = create_tmp_file(tmpname, file_size);
    if (fd < 0) {
        return -1;
    }

    vec = alloc_mincore_vec(file_size);
    if (vec == NULL) {
        goto err_alloc;
    }

    buf = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (buf == ((void *)-1)) {
        fprintf(stderr, "Failed to mmap file: %s\n", strerror(errno));
        goto err_mmap;
    }

    if (!check_caching((void *)buf, vec, file_size, false)) {
        goto err;
    }

    for (i = 0; i < test_runs; i++) {
        gettimeofday(&begin_time, NULL);
        //Read backwards to prevent mmap prefetching
        for (j = ((file_size - 1) & ~(pagesize - 1)); j >= 0; j -= pagesize) {
            buf[j];
        }
        gettimeofday(&end_time, NULL);

        timersub(&end_time, &begin_time, &elapsed_time);
        timeradd(&total_time_in, &elapsed_time, &total_time_in);

        if (!check_caching((void *)buf, vec, file_size, true)) {
            goto err;
        }

        gettimeofday(&begin_time, NULL);
        rc = madvise((void *)buf, file_size, MADV_DONTNEED) ||
               posix_fadvise(fd, 0, file_size, POSIX_FADV_DONTNEED);
        gettimeofday(&end_time, NULL);
        if (rc) {
            fprintf(stderr, "posix_fadvise/madvise DONTNEED failed\n");
            goto err;
        }

        timersub(&end_time, &begin_time, &elapsed_time);
        timeradd(&total_time_out, &elapsed_time, &total_time_out);

        if (!check_caching((void *)buf, vec, file_size, false)) {
            goto err;
        }
    }

    printf("page-in: %llu MB/s\n", (file_size * test_runs * USEC_PER_SEC) /
             (1024 * 1024 * (total_time_in.tv_sec * USEC_PER_SEC + total_time_in.tv_usec)));
    printf("page-out (clean): %llu MB/s\n", (file_size * test_runs * USEC_PER_SEC) /
             (1024 * 1024 * (total_time_out.tv_sec * USEC_PER_SEC + total_time_out.tv_usec)));

    ret = 0;

err:
    munmap((void *)buf, file_size);
err_mmap:
    free(vec);
err_alloc:
    close(fd);
    return ret;
}
