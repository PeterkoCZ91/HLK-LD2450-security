// Native Unity tests for MQTT offline buffer helpers. Run: pio test -e native
#include <unity.h>
#include "ld2450/utils/mqtt_buffer_logic.h"

using namespace ld2450_mqttbuf;

struct M { char topic[8]; uint8_t retained; };

void setUp(void) {}
void tearDown(void) {}

static void test_fits(void) {
    TEST_ASSERT_TRUE(fits("abc", 4));
    TEST_ASSERT_FALSE(fits("abcd", 4));   // would need 5 with NUL
    TEST_ASSERT_TRUE(fits("", 1));
    TEST_ASSERT_FALSE(fits(nullptr, 4));
}

static void test_find_retained(void) {
    M b[4] = { {"a",1}, {"b",0}, {"c",1}, {"b",1} };
    TEST_ASSERT_EQUAL_INT(0, findRetained(b, 4, 0, 4, "a"));
    TEST_ASSERT_EQUAL_INT(3, findRetained(b, 4, 0, 4, "b"));  // non-retained b skipped
    TEST_ASSERT_EQUAL_INT(-1, findRetained(b, 4, 0, 4, "z"));
}

static void test_find_wrapped_ring(void) {
    // head=2, count=3 -> live slots 2,3,0 ; slot 1 is stale garbage
    M b[4] = { {"x",1}, {"a",1}, {"y",1}, {"z",0} };
    TEST_ASSERT_EQUAL_INT(0, findRetained(b, 4, 2, 3, "x"));
    TEST_ASSERT_EQUAL_INT(-1, findRetained(b, 4, 2, 3, "a"));
    TEST_ASSERT_EQUAL_INT(-1, findRetained(b, 4, 0, 0, "x"));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_fits);
    RUN_TEST(test_find_retained);
    RUN_TEST(test_find_wrapped_ring);
    return UNITY_END();
}
