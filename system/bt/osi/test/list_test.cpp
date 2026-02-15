#include <gtest/gtest.h>

#include "AllocationTestHarness.h"

extern "C" {
#include "osi/include/list.h"
#include "osi/include/osi.h"
}

class ListTest : public AllocationTestHarness {};

TEST_F(ListTest, test_new_free_simple) {
  list_t *list = list_new(NULL);
  ASSERT_TRUE(list != NULL);
  list_free(list);
}

TEST_F(ListTest, test_free_null) {
  // In this test we just verify that list_free is callable with NULL.
  list_free(NULL);
}

TEST_F(ListTest, test_empty_list_is_empty) {
  list_t *list = list_new(NULL);
  EXPECT_TRUE(list_is_empty(list));
  list_free(list);
}

TEST_F(ListTest, test_empty_list_has_no_length) {
  list_t *list = list_new(NULL);
  EXPECT_EQ(list_length(list), 0U);
  list_free(list);
}

TEST_F(ListTest, test_simple_list_prepend) {
  list_t *list = list_new(NULL);
  EXPECT_TRUE(list_prepend(list, &list));
  EXPECT_FALSE(list_is_empty(list));
  EXPECT_EQ(list_length(list), 1U);
  list_free(list);
}

TEST_F(ListTest, test_simple_list_append) {
  list_t *list = list_new(NULL);
  EXPECT_TRUE(list_append(list, &list));
  EXPECT_FALSE(list_is_empty(list));
  EXPECT_EQ(list_length(list), 1U);
  list_free(list);
}

TEST_F(ListTest, test_list_remove_found) {
  list_t *list = list_new(NULL);
  list_append(list, &list);
  EXPECT_TRUE(list_remove(list, &list));
  EXPECT_TRUE(list_is_empty(list));
  EXPECT_EQ(list_length(list),  0U);
  list_free(list);
}

TEST_F(ListTest, test_list_remove_not_found) {
  int x;
  list_t *list = list_new(NULL);
  list_append(list, &list);
  EXPECT_FALSE(list_remove(list, &x));
  EXPECT_FALSE(list_is_empty(list));
  EXPECT_EQ(list_length(list), 1U);
  list_free(list);
}

TEST_F(ListTest, test_list_front) {
  int x[] = { 1, 2, 3, 4, 5 };
  list_t *list = list_new(NULL);

  for (size_t i = 0; i < ARRAY_SIZE(x); ++i)
    list_append(list, &x[i]);

  EXPECT_EQ(list_front(list), &x[0]);

  list_free(list);
}

TEST_F(ListTest, test_list_back) {
  int x[] = { 1, 2, 3, 4, 5 };
  list_t *list = list_new(NULL);

  for (size_t i = 0; i < ARRAY_SIZE(x); ++i)
    list_append(list, &x[i]);

  EXPECT_EQ(list_back(list), &x[ARRAY_SIZE(x) - 1]);

  list_free(list);
}

TEST_F(ListTest, test_list_clear) {
  int x[] = { 1, 2, 3, 4, 5 };
  list_t *list = list_new(NULL);

  for (size_t i = 0; i < ARRAY_SIZE(x); ++i)
    list_append(list, &x[i]);

  list_clear(list);
  EXPECT_TRUE(list_is_empty(list));
  EXPECT_EQ(list_length(list), 0U);

  list_free(list);
}

TEST_F(ListTest, test_list_append_multiple) {
  int x[] = { 1, 2, 3, 4, 5 };
  list_t *list = list_new(NULL);

  for (size_t i = 0; i < ARRAY_SIZE(x); ++i)
    list_append(list, &x[i]);

  int i = 0;
  for (const list_node_t *node = list_begin(list); node != list_end(list); node = list_next(node), ++i)
    EXPECT_EQ(list_node(node), &x[i]);

  list_free(list);
}

TEST_F(ListTest, test_list_prepend_multiple) {
  int x[] = { 1, 2, 3, 4, 5 };
  list_t *list = list_new(NULL);

  for (size_t i = 0; i < ARRAY_SIZE(x); ++i)
    list_prepend(list, &x[i]);

  int i = ARRAY_SIZE(x) - 1;
  for (const list_node_t *node = list_begin(list); node != list_end(list); node = list_next(node), --i)
    EXPECT_EQ(list_node(node), &x[i]);

  list_free(list);
}

TEST_F(ListTest, test_list_begin_empty_list) {
  list_t *list = list_new(NULL);
  EXPECT_EQ(list_begin(list), list_end(list));
  list_free(list);
}

TEST_F(ListTest, test_list_next) {
  list_t *list = list_new(NULL);
  list_append(list, &list);
  EXPECT_NE(list_begin(list), list_end(list));
  EXPECT_EQ(list_next(list_begin(list)), list_end(list));
  list_free(list);
}

static bool list_callback_sum(void *data, void *context) {
  assert(data);
  assert(context);
  int *sum = (int *)context;
  int *value = (int *)data;
  *sum += *value;
  return true;
}

static bool list_callback_find_int(void *data, void *context) {
  assert(data);
  assert(context);
  return (*(int *)data != *(int *)context);
}

TEST_F(ListTest, test_list_foreach_full) {
  list_t *list = list_new(NULL);

  // Fill in test data
  int x[] = { 1, 2, 3, 4, 5 };
  for (size_t i = 0; i < ARRAY_SIZE(x); ++i)
    list_append(list, &x[i]);
  EXPECT_EQ(list_length(list), (size_t)5);

  // Test complete iteration
  int sum = 0;
  list_node_t *rc = list_foreach(list, list_callback_sum, &sum);
  EXPECT_EQ(sum, 15);
  EXPECT_TRUE(rc == NULL);

  list_free(list);
}

TEST_F(ListTest, test_list_foreach_partial) {
  list_t *list = list_new(NULL);

  // Fill in test data
  int x[] = { 1, 2, 3, 4, 5 };
  for (size_t i = 0; i < ARRAY_SIZE(x); ++i)
    list_append(list, &x[i]);
  EXPECT_EQ(list_length(list), (size_t)5);

  // Test partial iteration
  int find = 4;
  list_node_t *rc = list_foreach(list, list_callback_find_int, &find);
  EXPECT_TRUE(rc != NULL);
  int *rc_val = (int *)list_node(rc);
  EXPECT_TRUE(*rc_val == 4);

  find = 1;
  rc = list_foreach(list, list_callback_find_int, &find);
  EXPECT_TRUE(rc != NULL);
  rc_val = (int *)list_node(rc);
  EXPECT_TRUE(*rc_val == 1);

  find = 5;
  rc = list_foreach(list, list_callback_find_int, &find);
  EXPECT_TRUE(rc != NULL);
  rc_val = (int *)list_node(rc);
  EXPECT_TRUE(*rc_val == 5);

  find = 0;
  rc = list_foreach(list, list_callback_find_int, &find);
  EXPECT_TRUE(rc == NULL);

  list_free(list);
}
