// Native Unity tests for the siren GPIO safe allowlist (DOC-03).
// Run: pio test -e native

#include <unity.h>
#include "ld2450/utils/gpio_allowlist.h"

using ld2450_gpio::sirenPinAllowed;

static void test_disabled_ok(void) {
    TEST_ASSERT_TRUE(sirenPinAllowed(-1));  // -1 = disabled is valid
}

static void test_safe_pins(void) {
    int ok[] = {4,5,13,14,16,17,21,22,23,25,26,27,32,33};
    for (int p : ok) TEST_ASSERT_TRUE(sirenPinAllowed(p));
}

static void test_reserved_rejected(void) {
    // flash, UART0, LED, radar UART, strapping, input-only, out of range
    int bad[] = {0,1,2,3,6,7,8,9,10,11,12,15,18,19,34,35,36,37,38,39,40,99,-2};
    for (int p : bad) TEST_ASSERT_FALSE(sirenPinAllowed(p));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_disabled_ok);
    RUN_TEST(test_safe_pins);
    RUN_TEST(test_reserved_rejected);
    return UNITY_END();
}
