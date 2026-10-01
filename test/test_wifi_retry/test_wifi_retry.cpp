#include <unity.h>
#include "ld2450/utils/wifi_retry.h"
using namespace ld2450_wifi;
void setUp(void) {}
void tearDown(void) {}
static void test_alternates(void) {
    TEST_ASSERT_TRUE(nextIsBackup(false, true, true));
    TEST_ASSERT_FALSE(nextIsBackup(true, true, true));
}
static void test_single(void) {
    TEST_ASSERT_FALSE(nextIsBackup(false, true, false));
    TEST_ASSERT_FALSE(nextIsBackup(true, true, false));
    TEST_ASSERT_TRUE(nextIsBackup(true, false, true));
    TEST_ASSERT_TRUE(nextIsBackup(false, false, true));
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_alternates);
    RUN_TEST(test_single);
    return UNITY_END();
}
