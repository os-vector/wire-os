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
#include <stdint.h>
#include <unistd.h>

#include "osi/include/allocator.h"
#include "osi/include/eager_reader.h"
#include "osi/include/osi.h"
#include "osi/include/semaphore.h"
#include "osi/include/thread.h"
}

#define BUFFER_SIZE 32

static const char *small_data = "white chocolate lindor truffles";
static const char *large_data =
  "Let him make him examine and thoroughly sift everything he reads, and "
  "lodge nothing in his fancy upon simple authority and upon trust. "
  "Aristotle's principles will then be no more principles to him, than those "
  "of Epicurus and the Stoics: let this diversity of opinions be propounded "
  "to, and laid before him; he will himself choose, if he be able; if not, "
  "he will remain in doubt. "
  ""
  "   \"Che non men the saver, dubbiar m' aggrata.\" "
  "   [\"I love to doubt, as well as to know.\"--Dante, Inferno, xi. 93] "
  ""
  "for, if he embrace the opinions of Xenophon and Plato, by his own reason, "
  "they will no more be theirs, but become his own.  Who follows another, "
  "follows nothing, finds nothing, nay, is inquisitive after nothing. "
  ""
  "   \"Non sumus sub rege; sibi quisque se vindicet.\" "
  "   [\"We are under no king; let each vindicate himself.\" --Seneca, Ep.,33] "
  ""
  "let him, at least, know that he knows.  it will be necessary that he "
  "imbibe their knowledge, not that he be corrupted with their precepts; "
  "and no matter if he forget where he had his learning, provided he know "
  "how to apply it to his own use.  truth and reason are common to every "
  "one, and are no more his who spake them first, than his who speaks them "
  "after: 'tis no more according to plato, than according to me, since both "
  "he and i equally see and understand them.  bees cull their several sweets "
  "from this flower and that blossom, here and there where they find them, "
  "but themselves afterwards make the honey, which is all and purely their "
  "own, and no more thyme and marjoram: so the several fragments he borrows "
  "from others, he will transform and shuffle together to compile a work "
  "that shall be absolutely his own; that is to say, his judgment: "
  "his instruction, labour and study, tend to nothing else but to form that. ";

static semaphore_t *done;

class EagerReaderTest : public AllocationTestHarness {
  protected:
    virtual void SetUp() {
      AllocationTestHarness::SetUp();
      pipe(pipefd);
      done = semaphore_new(0);
    }

    virtual void TearDown() {
      semaphore_free(done);
      AllocationTestHarness::TearDown();
    }

    int pipefd[2];
};

static void expect_data(eager_reader_t *reader, void *context) {
  char *data = (char *)context;
  int length = strlen(data);

  for (int i = 0; i < length; i++) {
    uint8_t byte;
    EXPECT_EQ((size_t)1, eager_reader_read(reader, &byte, 1));
    EXPECT_EQ(data[i], byte);
  }

  semaphore_post(done);
}

static void expect_data_multibyte(eager_reader_t *reader, void *context) {
  char *data = (char *)context;
  size_t length = strlen(data);

  for (size_t i = 0; i < length;) {
    uint8_t buffer[28];
    size_t bytes_to_read = (length - i) > 28 ? 28 : (length - i);
    size_t bytes_read = eager_reader_read(reader, buffer, bytes_to_read);
    EXPECT_LE(bytes_read, bytes_to_read);
    for (size_t j = 0; j < bytes_read && i < length; j++, i++) {
      EXPECT_EQ(data[i], buffer[j]);
    }
  }

  semaphore_post(done);
}

TEST_F(EagerReaderTest, test_new_free_simple) {
  eager_reader_t *reader = eager_reader_new(pipefd[0], &allocator_malloc, BUFFER_SIZE, SIZE_MAX, "test_thread");
  ASSERT_TRUE(reader != NULL);
  eager_reader_free(reader);
}

TEST_F(EagerReaderTest, test_small_data) {
  eager_reader_t *reader = eager_reader_new(pipefd[0], &allocator_malloc, BUFFER_SIZE, SIZE_MAX, "test_thread");

  thread_t *read_thread = thread_new("read_thread");
  eager_reader_register(reader, thread_get_reactor(read_thread), expect_data, (void *)small_data);

  write(pipefd[1], small_data, strlen(small_data));

  semaphore_wait(done);
  eager_reader_free(reader);
  thread_free(read_thread);
}

TEST_F(EagerReaderTest, test_large_data_multibyte) {
  eager_reader_t *reader = eager_reader_new(pipefd[0], &allocator_malloc, BUFFER_SIZE, SIZE_MAX, "test_thread");

  thread_t *read_thread = thread_new("read_thread");
  eager_reader_register(reader, thread_get_reactor(read_thread), expect_data_multibyte, (void *)large_data);

  write(pipefd[1], large_data, strlen(large_data));

  semaphore_wait(done);
  eager_reader_free(reader);
  thread_free(read_thread);
}
