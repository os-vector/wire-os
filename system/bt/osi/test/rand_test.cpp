#include <gtest/gtest.h>

#include "AllocationTestHarness.h"

extern "C" {
#include "osi/include/osi.h"
}

class RandTest : public AllocationTestHarness {};

TEST_F(RandTest, test_rand) {
  // We can't guarantee any distribution
  // We'd like it to not crash though.
  for (int i = 0; i < 10; i++) {
    int x;
    x = osi_rand();
    EXPECT_TRUE(x >= 0);
  }
}
