#include <unity.h>
#include "ld2450/utils/presence_logic.h"
using namespace ld2450_presence;

void setUp(void) {}
void tearDown(void) {}

static void test_time_reached_wrap(void) {
    TEST_ASSERT_TRUE(timeReached(1000, 1000));
    TEST_ASSERT_FALSE(timeReached(999, 1000));
    // deadline wrapped past 2^32, now not yet
    TEST_ASSERT_FALSE(timeReached(0xFFFFFF00u, 0xFFFFFF00u + 30000u));
    TEST_ASSERT_TRUE(timeReached(30000u - 256u, (uint32_t)(0xFFFFFF00u + 30000u)));
}

static void test_tamper_clears_after_continuous_visibility(void) {
    TamperClear tc;
    uint32_t seen = 1000;
    // targets visible every 100 ms (lastTargetSeen updated each frame)
    bool cleared = false;
    for (uint32_t t = 1100; t <= 8000 && !cleared; t += 100) {
        cleared = tc.update(true, 1, t, seen, 5000, 10000);
        seen = t;
        if (cleared) TEST_ASSERT_TRUE(t > 6100 && t <= 6300);
    }
    TEST_ASSERT_TRUE(cleared);
}

static void test_tamper_visibility_reset_by_gap(void) {
    TamperClear tc;
    TEST_ASSERT_FALSE(tc.update(true, 1, 1000, 0, 5000, 10000));
    TEST_ASSERT_FALSE(tc.update(true, 0, 3000, 1000, 5000, 10000)); // gap
    TEST_ASSERT_FALSE(tc.update(true, 1, 4000, 1000, 5000, 10000)); // restart
    TEST_ASSERT_FALSE(tc.update(true, 1, 8000, 4000, 5000, 10000));
    TEST_ASSERT_TRUE(tc.update(true, 1, 9100, 8000, 5000, 10000));
}

static void test_tamper_clears_when_absent(void) {
    TamperClear tc;
    TEST_ASSERT_FALSE(tc.update(true, 0, 10000, 1000, 5000, 10000));
    TEST_ASSERT_TRUE(tc.update(true, 0, 11001, 1000, 5000, 10000));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_time_reached_wrap);
    RUN_TEST(test_tamper_clears_after_continuous_visibility);
    RUN_TEST(test_tamper_visibility_reset_by_gap);
    RUN_TEST(test_tamper_clears_when_absent);
    return UNITY_END();
}
