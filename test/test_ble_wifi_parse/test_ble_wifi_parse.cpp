#include <unity.h>
#include "ld2450/utils/ble_wifi_parse.h"
using namespace ld2450_ble;
void setUp(void) {}
void tearDown(void) {}

static void test_simple(void) {
    std::string s, p;
    TEST_ASSERT_TRUE(splitWifiCred("home,secret", s, p));
    TEST_ASSERT_EQUAL_STRING("home", s.c_str());
    TEST_ASSERT_EQUAL_STRING("secret", p.c_str());
}
static void test_comma_in_ssid(void) {
    std::string s, p;
    TEST_ASSERT_TRUE(splitWifiCred("Net, 5G,pass1234", s, p));
    TEST_ASSERT_EQUAL_STRING("Net, 5G", s.c_str());
    TEST_ASSERT_EQUAL_STRING("pass1234", p.c_str());
}
static void test_open_and_invalid(void) {
    std::string s, p;
    TEST_ASSERT_TRUE(splitWifiCred("open,", s, p));
    TEST_ASSERT_EQUAL_STRING("", p.c_str());
    TEST_ASSERT_FALSE(splitWifiCred("nocomma", s, p));
    TEST_ASSERT_FALSE(splitWifiCred(",pw", s, p));
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_simple);
    RUN_TEST(test_comma_in_ssid);
    RUN_TEST(test_open_and_invalid);
    return UNITY_END();
}
