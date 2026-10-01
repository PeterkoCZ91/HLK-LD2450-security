// Native Unity tests for the SEC-03 credential predicates.
// Spuštění: pio test -e native

#include <unity.h>
#include "ld2450/utils/credential_check.h"

using namespace ld2450;

void setUp(void) {}
void tearDown(void) {}

// --- isDefaultCreds ---

void test_admin_admin_is_default(void) {
    TEST_ASSERT_TRUE(isDefaultCreds("admin", "admin", "admin", "admin"));
}

void test_empty_creds_are_default(void) {
    TEST_ASSERT_TRUE(isDefaultCreds("", "", "admin", "admin"));
    TEST_ASSERT_TRUE(isDefaultCreds("admin", "", "admin", "admin"));
    TEST_ASSERT_TRUE(isDefaultCreds("", "secret", "admin", "admin"));
    TEST_ASSERT_TRUE(isDefaultCreds(nullptr, nullptr, "admin", "admin"));
}

void test_changed_creds_not_default(void) {
    TEST_ASSERT_FALSE(isDefaultCreds("admin", "Str0ngPass", "admin", "admin"));
    TEST_ASSERT_FALSE(isDefaultCreds("petr", "Str0ngPass", "admin", "admin"));
    // Same password but different user is not the shipped default pair.
    TEST_ASSERT_FALSE(isDefaultCreds("petr", "admin", "admin", "admin"));
}

// --- isAcceptableNewPassword ---

void test_empty_password_rejected(void) {
    TEST_ASSERT_FALSE(isAcceptableNewPassword(""));
    TEST_ASSERT_FALSE(isAcceptableNewPassword(nullptr));
}

void test_nonempty_password_accepted(void) {
    TEST_ASSERT_TRUE(isAcceptableNewPassword("x"));
    TEST_ASSERT_TRUE(isAcceptableNewPassword("Str0ngPass"));
    // Light policy by design: even "admin" is accepted (only empty is blocked).
    TEST_ASSERT_TRUE(isAcceptableNewPassword("admin"));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_admin_admin_is_default);
    RUN_TEST(test_empty_creds_are_default);
    RUN_TEST(test_changed_creds_not_default);
    RUN_TEST(test_empty_password_rejected);
    RUN_TEST(test_nonempty_password_accepted);
    return UNITY_END();
}
