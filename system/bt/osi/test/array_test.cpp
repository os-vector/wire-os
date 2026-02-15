#include <gtest/gtest.h>

#include "AllocationTestHarness.h"

extern "C" {
#include "osi/include/array.h"
}

class ArrayTest : public AllocationTestHarness {};

TEST_F(ArrayTest, test_new_free_simple) {
  array_t *array = array_new(4);
  ASSERT_TRUE(array != NULL);
  array_free(array);
}

TEST_F(ArrayTest, test_free_null) {
  array_free(NULL);
}

TEST_F(ArrayTest, test_invalid_ptr) {
  array_t *array = array_new(4);
  EXPECT_DEATH(array_ptr(array), "");
  array_free(array);
}

TEST_F(ArrayTest, test_invalid_at) {
  array_t *array = array_new(4);
  EXPECT_DEATH(array_at(array, 1), "");
  array_free(array);
}

TEST_F(ArrayTest, test_append_value) {
  array_t *array = array_new(sizeof(int));
  for (int i = 0; i < 100; ++i) {
    array_append_value(array, i * i);
  }
  for (int i = 0; i < 100; ++i) {
    EXPECT_EQ(*(int *)array_at(array, i), i * i);
  }
  array_free(array);
}

TEST_F(ArrayTest, test_append_ptr) {
  int items[100];
  array_t *array = array_new(sizeof(int));
  for (int i = 0; i < 100; ++i) {
    items[i] = i * i;
    array_append_ptr(array, &items[i]);
  }
  for (int i = 0; i < 100; ++i) {
    EXPECT_EQ(*(int *)array_at(array, i), i * i);
  }
  array_free(array);
}

TEST_F(ArrayTest, test_large_element) {
  char strings[][128] = {
    "string 1",
    "string 2",
    "string 3",
    "string 4",
    "string 5",
    "string 6",
    "string 7",
    "string 8",
  };

  array_t *array = array_new(128);
  for (int i = 0; i < 100; ++i) {
    array_append_ptr(array, strings[i % 8]);
  }
  for (int i = 0; i < 100; ++i) {
    EXPECT_TRUE(!memcmp(array_at(array, i), strings[i % 8], 128));
  }
  array_free(array);
}
