/******************************************************************************
 *
 *  Copyright (C) 2014 Google, Inc.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

#include <gtest/gtest.h>

#include "AllocationTestHarness.h"

extern "C" {
#include "osi/include/hash_map.h"
#include "osi/include/osi.h"
}

class HashMapTest : public AllocationTestHarness {};

hash_index_t hash_map_fn00(const void *key) {
  hash_index_t hash_key = (hash_index_t)key;
  return hash_key;
}

static size_t g_key_free;
void key_free_fn00(UNUSED_ATTR void *data) {
  g_key_free++;
}

static size_t g_data_free;
void data_free_fn00(UNUSED_ATTR void *data) {
  g_data_free++;
}

TEST_F(HashMapTest, test_new_free_simple) {
  hash_map_t *hash_map = hash_map_new(5, hash_map_fn00, NULL, NULL, NULL);
  ASSERT_TRUE(hash_map != NULL);
  hash_map_free(hash_map);
}

TEST_F(HashMapTest, test_insert_simple) {
  hash_map_t *hash_map = hash_map_new(5, hash_map_fn00, NULL, NULL, NULL);
  ASSERT_TRUE(hash_map != NULL);

  struct {
    const char *key;
    const char *data;
  } data[] = {
    { "0", "zero" },
    { "1", "one" },
    { "2", "two" },
    { "3", "three" },
  };

  size_t data_sz = sizeof(data)/sizeof(data[0]);

  for (size_t i = 0; i < data_sz; i++) {
    EXPECT_EQ(i, hash_map_size(hash_map));
    hash_map_set(hash_map, data[i].key, (void*)data[i].data);
    EXPECT_EQ(i + 1, hash_map_size(hash_map));
  }

  EXPECT_EQ(data_sz, hash_map_size(hash_map));

  for (size_t i = 0; i < data_sz; i++) {
    char *val = (char *)hash_map_get(hash_map, data[i].key);
    EXPECT_STREQ(data[i].data, val);
  }
  EXPECT_EQ(data_sz, hash_map_size(hash_map));

  hash_map_free(hash_map);
}

TEST_F(HashMapTest, test_insert_same) {
  hash_map_t *hash_map = hash_map_new(5, hash_map_fn00, NULL, NULL, NULL);
  ASSERT_TRUE(hash_map != NULL);

  struct {
    const char *key;
    const char *data;
  } data[] = {
    { "0", "zero" },
    { "0", "one" },
    { "0", "two" },
    { "0", "three" },
  };

  size_t data_sz = sizeof(data)/sizeof(data[0]);

  for (size_t i = 0; i < data_sz; i++) {
    hash_map_set(hash_map, data[i].key, (void*)data[i].data);
    EXPECT_EQ(1U, hash_map_size(hash_map));
  }

  EXPECT_EQ(1U, hash_map_size(hash_map));

  for (size_t i = 0; i < data_sz; i++) {
    char *val = (char *)hash_map_get(hash_map, data[i].key);
    EXPECT_STREQ(data[data_sz - 1].data, val);
  }

  hash_map_free(hash_map);
}

TEST_F(HashMapTest, test_functions) {
  hash_map_t *hash_map = hash_map_new(5, hash_map_fn00, key_free_fn00, data_free_fn00, NULL);
  ASSERT_TRUE(hash_map != NULL);

  struct {
    const char *key;
    const char *data;
  } data[] = {
    { "0", "zero" },
    { "1", "one" },
    { "2", "two" },
    { "3", "three" },
  };

  g_data_free = 0;
  g_key_free = 0;

  size_t data_sz = sizeof(data)/sizeof(data[0]);

  for (size_t i = 0; i < data_sz; i++) {
    EXPECT_EQ(hash_map_size(hash_map), i);
    hash_map_set(hash_map, data[i].key, (void*)data[i].data);
  }

  EXPECT_EQ(data_sz, hash_map_size(hash_map));
  EXPECT_EQ((size_t)0, g_data_free);
  EXPECT_EQ((size_t)0, g_key_free);

  for (size_t i = 0; i < data_sz; i++) {
    char *val = (char *)hash_map_get(hash_map, data[i].key);
    EXPECT_TRUE(val != NULL);
    EXPECT_STREQ(data[i].data, val);
    hash_map_erase(hash_map, (void*)data[i].key);
    EXPECT_EQ(i + 1, g_data_free);
    EXPECT_EQ(i + 1, g_key_free);
  }

  hash_map_free(hash_map);
}

struct hash_test_iter_data_s {
  const char *key;
  const char *data;
} hash_test_iter_data[] = {
  { "0", "zero" },
  { "1", "one" },
  { "2", "two" },
  { "3", "three" },
  { "elephant", "big" },
  { "fox", "medium" },
  { "gerbil", "small" },
};

bool hash_test_iter_ro_cb(hash_map_entry_t *hash_map_entry, void *context) {
  const char *key = (const char *)hash_map_entry->key;
  char *data = (char *)hash_map_entry->data;
  EXPECT_TRUE(data != NULL);

  size_t hash_test_iter_data_sz = sizeof(hash_test_iter_data)/sizeof(hash_test_iter_data[0]);
  size_t i;
  for (i = 0; i < hash_test_iter_data_sz; i++) {
    if (!strcmp(hash_test_iter_data[i].key, key))
      break;
  }
  EXPECT_NE(hash_test_iter_data_sz, i);
  EXPECT_EQ(NULL, context);
  EXPECT_STREQ(hash_test_iter_data[i].data, data);
  return true;
}

TEST_F(HashMapTest, test_iter) {
  hash_map_t *hash_map = hash_map_new(5, hash_map_fn00, key_free_fn00, data_free_fn00, NULL);
  ASSERT_TRUE(hash_map != NULL);
  g_data_free = 0;
  g_key_free = 0;

  size_t hash_test_iter_data_sz = sizeof(hash_test_iter_data)/sizeof(hash_test_iter_data[0]);

  for (size_t i = 0; i < hash_test_iter_data_sz; i++) {
    EXPECT_EQ(hash_map_size(hash_map), i);
    hash_map_set(hash_map, hash_test_iter_data[i].key, (void*)hash_test_iter_data[i].data);
  }

  void *context = NULL;
  hash_map_foreach(hash_map, hash_test_iter_ro_cb, context);

  hash_map_free(hash_map);
}
