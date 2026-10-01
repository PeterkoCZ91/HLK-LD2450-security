// Native Unity tests for wraparound-safe timing helpers. Run: pio test -e native
#include <unity.h>
#include "ld2450/utils/timing.h"

using namespace ld2450_timing;

void setUp(void) {}
void tearDown(void) {}

static void test_elapsed_basic(void) {
    TEST_ASSERT_TRUE(elapsedAtLeast(1000, 0, 1000));
    TEST_ASSERT_FALSE(elapsedAtLeast(999, 0, 1000));
}

static void test_elapsed_wraparound(void) {
    // start just before 2^32, now just after wrap: 20 ms elapsed
    TEST_ASSERT_TRUE(elapsedAtLeast(10, 0xFFFFFFF6u, 20));
    TEST_ASSERT_FALSE(elapsedAtLeast(10, 0xFFFFFFF6u, 21));
}

static void test_elapsed_start_in_future(void) {
    // now sampled before another task stored start (exit delay must NOT expire)
    TEST_ASSERT_FALSE(elapsedAtLeast(1000, 1005, 30000));
}

static void test_cooldown(void) {
    TEST_ASSERT_TRUE(cooldownElapsed(5000, 0, 60000));      // never fired
    TEST_ASSERT_FALSE(cooldownElapsed(5000, 4000, 60000));
    TEST_ASSERT_TRUE(cooldownElapsed(70000, 4000, 60000));
    TEST_ASSERT_TRUE(cooldownElapsed(100, 0xFFFFFF00u, 100)); // wrap: 356 ms
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_elapsed_basic);
    RUN_TEST(test_elapsed_wraparound);
    RUN_TEST(test_elapsed_start_in_future);
    RUN_TEST(test_cooldown);
    return UNITY_END();
}
