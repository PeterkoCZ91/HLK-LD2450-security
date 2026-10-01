#include <unity.h>
#include "ld2450/utils/sched_edge.h"
using namespace ld2450_sched;
void setUp(void) {}
void tearDown(void) {}

static void test_edge_once_per_minute(void) {
    int32_t last = -1;
    TEST_ASSERT_TRUE(edgeOnce(true, 100, last));
    TEST_ASSERT_FALSE(edgeOnce(true, 100, last));  // 30 s later, same minute
    TEST_ASSERT_FALSE(edgeOnce(false, 101, last));
    TEST_ASSERT_TRUE(edgeOnce(true, 100 + 1440, last));  // next day
}
static void test_cert_first_check(void) {
    TEST_ASSERT_FALSE(certCheckDue(5000, 0, false, 86400000u, false));
    TEST_ASSERT_TRUE(certCheckDue(5000, 0, false, 86400000u, true));
    TEST_ASSERT_FALSE(certCheckDue(6000, 5000, true, 86400000u, true));
    TEST_ASSERT_TRUE(certCheckDue(5000u + 86400000u, 5000, true, 86400000u, true));
    TEST_ASSERT_TRUE(certCheckDue(10, 0xFFFFFFF6u, true, 20, true));  // wrap
}
static void test_retry(void) {
    TEST_ASSERT_TRUE(retryDue(1000, 0, 60000));
    TEST_ASSERT_FALSE(retryDue(30000, 1000, 60000));
    TEST_ASSERT_TRUE(retryDue(61000, 1000, 60000));
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_edge_once_per_minute);
    RUN_TEST(test_cert_first_check);
    RUN_TEST(test_retry);
    return UNITY_END();
}
