#include <unistd.h>
#include <stdbool.h>

#ifndef __PAGINGTEST_H__
#define __PAGINGTEST_H__
#define USEC_PER_SEC 1000000ULL
#define mincore_vec_len(size) (((size) + sysconf(_SC_PAGE_SIZE) - 1) / sysconf(_SC_PAGE_SIZE))
#define ARRAY_SIZE(a) (sizeof(a) / sizeof(*(a)))

//Helpers
int create_tmp_file(char *filename, off_t size);
unsigned char *alloc_mincore_vec(size_t size);
bool check_caching(void *buf, unsigned char *vec, size_t size, bool is_cached);

//Tests
int mmap_test(int test_runs, unsigned long long alloc_size);
int pageinout_test(int test_runs, unsigned long long file_size);
int thrashing_test(int test_runs);

#endif //__PAGINGTEST_H__
