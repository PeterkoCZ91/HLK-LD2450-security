// Native Unity tests for web parameter range checks. Run: pio test -e native
#include <unity.h>
#include "ld2450/utils/web_params.h"

using namespace ld2450;

void setUp(void) {}
void tearDown(void) {}

static void test_seconds(void) {
    unsigned long ms = 7;
    TEST_ASSERT_TRUE(secondsToMs(30, 600, ms));
    TEST_ASSERT_EQUAL_UINT32(30000, ms);
    TEST_ASSERT_TRUE(secondsToMs(0, 600, ms));
    TEST_ASSERT_FALSE(secondsToMs(-1, 600, ms));   // would wrap to ~4e12 ms
    TEST_ASSERT_FALSE(secondsToMs(601, 600, ms));
}

static void test_idx(void) {
    TEST_ASSERT_TRUE(idxInRange(0, 4));
    TEST_ASSERT_TRUE(idxInRange(3, 4));
    TEST_ASSERT_FALSE(idxInRange(4, 4));
    TEST_ASSERT_FALSE(idxInRange(256, 4));   // (uint8_t)256 == 0
    TEST_ASSERT_FALSE(idxInRange(-1, 4));
}

static void test_coord(void) {
    TEST_ASSERT_TRUE(coordInRange(-10000));
    TEST_ASSERT_TRUE(coordInRange(10000));
    TEST_ASSERT_FALSE(coordInRange(10001));
    TEST_ASSERT_FALSE(coordInRange(70000));  // (int16_t) cast would wrap
}

static void test_fits(void) {
    TEST_ASSERT_TRUE(fitsBuf("admin", 20, false));
    TEST_ASSERT_FALSE(fitsBuf("", 20, false));
    TEST_ASSERT_TRUE(fitsBuf("", 20, true));
    TEST_ASSERT_TRUE(fitsBuf("1234567890123456789", 20, false));   // 19
    TEST_ASSERT_FALSE(fitsBuf("12345678901234567890", 20, false)); // 20
    TEST_ASSERT_FALSE(fitsBuf(nullptr, 20, true));
}

static void test_port(void) {
    TEST_ASSERT_TRUE(portTextValid(""));
    TEST_ASSERT_TRUE(portTextValid("1883"));
    TEST_ASSERT_TRUE(portTextValid("65535"));
    TEST_ASSERT_FALSE(portTextValid("65536"));
    TEST_ASSERT_FALSE(portTextValid("0"));
    TEST_ASSERT_FALSE(portTextValid("18a3"));
    TEST_ASSERT_FALSE(portTextValid("-1"));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_seconds);
    RUN_TEST(test_idx);
    RUN_TEST(test_coord);
    RUN_TEST(test_fits);
    RUN_TEST(test_port);
    return UNITY_END();
}
