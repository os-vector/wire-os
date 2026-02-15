#include "pagingtest.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define TEST_RUNS 10
#define ALLOC_SIZE (10 * 1024 * 1024)
#define FILE_SIZE (10 * 1024 * 1024)

int create_tmp_file(char *filename, off_t size) {
    void *buf;
    ssize_t rc;
    int fd;
    int urandom;

    fd = mkstemp(filename);
    if (fd < 0) {
        fprintf(stderr, "unable to create temp file: %s\n", strerror(errno));
        goto err_mkstemp;
    }

    urandom = open("/dev/urandom", O_RDONLY);
    if (urandom < 0) {
        fprintf(stderr, "unable to open urandom: %s\n", strerror(errno));
        goto err_open;
    }

    if (unlink(filename)) {
        fprintf(stderr, "unable to unlink temp file: %s\n", strerror(errno));
        goto err_unlink;
    }

    if (ftruncate(fd, size)) {
        fprintf(stderr, "unable to allocate temp file: %s\n", strerror(errno));
        goto err_truncate;
    }

    buf = mmap(NULL, size, PROT_WRITE, MAP_SHARED, fd, 0);
    if (buf == (void *)-1) {
        fprintf(stderr, "unable to mmap temp file: %s\n", strerror(errno));
        goto err_mmap;
    }

    rc = read(urandom, buf, size);

    if (rc < 0) {
        fprintf(stderr, "write random data failed: %s\n", strerror(errno));
        goto err;
    }

    if (rc != size) {
        fprintf(stderr, "write random data incomplete\n");
        goto err;
    }

    if (madvise(buf, size, MADV_DONTNEED)) {
        fprintf(stderr, "madvise DONTNEED failed: %s\n", strerror(errno));
        goto err;
    }

    if (fsync(fd) < 0) {
        fprintf(stderr, "fsync failed: %s\n", strerror(errno));
        goto err;
    }

    rc = posix_fadvise(fd, 0, size, POSIX_FADV_DONTNEED);
    if (rc) {
        fprintf(stderr, "fadvise DONTNEED failed: %s\n", strerror(errno));
        goto err;
    }

    munmap(buf, size);
    close(urandom);
    return fd;

err:
    munmap(buf, size);
err_mmap:
err_truncate:
err_unlink:
    close(urandom);
err_open:
    close(fd);
err_mkstemp:
    return -1;
}

unsigned char *alloc_mincore_vec(size_t size) {
    unsigned char *vec;

    vec = malloc(mincore_vec_len(size));
    if (vec == NULL) {
        fprintf(stderr, "malloc failed\n");
    }

    return vec;
}

bool check_caching(void *buf, unsigned char *vec, size_t size, bool is_cached) {
    bool ret = true;
    size_t i;

    if (mincore(buf, size, vec)) {
        fprintf(stderr, "mincore failed: %s\n", strerror(errno));
        return false;
    }

    if (is_cached) {
        for (i = 0; i < mincore_vec_len(size); i++) {
            if (!(vec[i] & 0x1)) {
                fprintf(stderr, "found an uncached page at page offset %zd\n", i);
                ret = false;
            }
        }
    } else {
        for (i = 0; i < mincore_vec_len(size); i++) {
            if (vec[i] & 0x1) {
                fprintf(stderr, "found a cached page at page offset %zd\n", i);
                ret = false;
            }
        }
    }

    return ret;
}

int main(int argc, char **argv) {
    unsigned long long alloc_size = 0ULL;
    unsigned long long file_size = 0ULL;
    int test_runs = 0;
    int rc;

    //arguments: <program> [test_runs [alloc_size [file_size]]]
    if (argc >= 2) {
        test_runs = atoi(argv[1]);
    }
    if (test_runs <= 0) {
        test_runs = TEST_RUNS;
    }
    if (argc >= 3) {
        alloc_size = strtoull(argv[2], NULL, 10);
    }
    if (!alloc_size) {
        alloc_size = ALLOC_SIZE;
    }
    if (argc >= 4) {
        file_size = strtoull(argv[3], NULL, 10);
    }
    if (!file_size) {
        file_size = FILE_SIZE;
    }

    rc = mmap_test(test_runs, alloc_size);
    if (rc) {
        return rc;
    }
    rc = pageinout_test(test_runs, file_size);
    if (rc) {
        return rc;
    }
    rc = thrashing_test(test_runs);

    return rc;
}
