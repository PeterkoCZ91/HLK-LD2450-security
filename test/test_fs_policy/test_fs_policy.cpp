#include <unity.h>
#include "ld2450/utils/fs_policy.h"

using namespace ld2450_fs;

void setUp() {}
void tearDown() {}

void test_fresh_flash_formats() {
    TEST_ASSERT_EQUAL(Format, mountFailAction(false, 0));
}
void test_single_failure_on_used_fs_skips() {
    TEST_ASSERT_EQUAL(Skip, mountFailAction(true, 0));
    TEST_ASSERT_EQUAL(Skip, mountFailAction(true, 1));
}
void test_repeated_failure_formats() {
    TEST_ASSERT_EQUAL(Format, mountFailAction(true, 2));
    TEST_ASSERT_EQUAL(Format, mountFailAction(true, 10));
}
void test_counter_overflow_safe() {
    TEST_ASSERT_EQUAL(Format, mountFailAction(true, 255));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_fresh_flash_formats);
    RUN_TEST(test_single_failure_on_used_fs_skips);
    RUN_TEST(test_repeated_failure_formats);
    RUN_TEST(test_counter_overflow_safe);
    return UNITY_END();
}
