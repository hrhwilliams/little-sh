#include "fuzztest/fuzztest.h"
#include "gtest/gtest.h"

void IntegerAdditionCommutes(int a, int b) {
    EXPECT_EQ(a+b, b+a);
}

FUZZ_TEST(QuashTestSuite, IntegerAdditionCommutes)
    .WithDomains(/*number=*/fuzztest::Arbitrary<int>(), /*suffix=*/fuzztest::InRegexp("[^0-9].*"));
