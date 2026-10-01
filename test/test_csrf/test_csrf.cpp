// Native Unity tests for the SEC-06 CSRF decision logic.
// Spuštění: pio test -e native

#include <unity.h>
#include "ld2450/utils/csrf_check.h"

using namespace ld2450;

void setUp(void) {}
void tearDown(void) {}

// --- csrfHostMatches ---

void test_host_matches_origin_no_path(void) {
    TEST_ASSERT_TRUE(csrfHostMatches("http://192.0.2.22", "192.0.2.22"));
    TEST_ASSERT_TRUE(csrfHostMatches("https://ld2450.local", "ld2450.local"));
}

void test_host_matches_referer_with_path(void) {
    TEST_ASSERT_TRUE(csrfHostMatches("http://192.0.2.22/index.html", "192.0.2.22"));
    TEST_ASSERT_TRUE(csrfHostMatches("http://host:8080/a/b?c=d", "host:8080"));
}

void test_host_matches_case_insensitive(void) {
    TEST_ASSERT_TRUE(csrfHostMatches("http://LD2450.Local/", "ld2450.local"));
}

void test_host_mismatch_rejected(void) {
    TEST_ASSERT_FALSE(csrfHostMatches("http://evil.example.com/", "192.0.2.22"));
    // Prefix must not be treated as a match.
    TEST_ASSERT_FALSE(csrfHostMatches("http://192.0.2.22.evil.com/", "192.0.2.22"));
    TEST_ASSERT_FALSE(csrfHostMatches("http://192.0.2.2", "192.0.2.22"));
}

void test_host_no_scheme_rejected(void) {
    TEST_ASSERT_FALSE(csrfHostMatches("192.0.2.22", "192.0.2.22"));
}

// --- csrfAllowed ---

void test_get_always_allowed(void) {
    // Non-mutating method: allowed even with a foreign origin.
    TEST_ASSERT_TRUE(csrfAllowed(false, "192.0.2.22", "http://evil.com", nullptr));
}

void test_same_origin_post_allowed(void) {
    TEST_ASSERT_TRUE(csrfAllowed(true, "192.0.2.22", "http://192.0.2.22", nullptr));
}

void test_cross_origin_post_blocked(void) {
    TEST_ASSERT_FALSE(csrfAllowed(true, "192.0.2.22", "http://evil.com", nullptr));
}

void test_falls_back_to_referer(void) {
    // No Origin, but same-origin Referer → allowed.
    TEST_ASSERT_TRUE(csrfAllowed(true, "192.0.2.22", nullptr, "http://192.0.2.22/app"));
    // No Origin, cross-origin Referer → blocked.
    TEST_ASSERT_FALSE(csrfAllowed(true, "192.0.2.22", nullptr, "http://evil.com/app"));
    // Empty-string headers treated as absent.
    TEST_ASSERT_FALSE(csrfAllowed(true, "192.0.2.22", "", "http://evil.com/app"));
}

void test_non_browser_client_allowed(void) {
    // No Origin and no Referer (curl / Home Assistant) → allowed.
    TEST_ASSERT_TRUE(csrfAllowed(true, "192.0.2.22", nullptr, nullptr));
    TEST_ASSERT_TRUE(csrfAllowed(true, "192.0.2.22", "", ""));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_host_matches_origin_no_path);
    RUN_TEST(test_host_matches_referer_with_path);
    RUN_TEST(test_host_matches_case_insensitive);
    RUN_TEST(test_host_mismatch_rejected);
    RUN_TEST(test_host_no_scheme_rejected);
    RUN_TEST(test_get_always_allowed);
    RUN_TEST(test_same_origin_post_allowed);
    RUN_TEST(test_cross_origin_post_blocked);
    RUN_TEST(test_falls_back_to_referer);
    RUN_TEST(test_non_browser_client_allowed);
    return UNITY_END();
}
