// Native Unity tests for schedule_time HH:MM parsing + night-window logic (TIME-01).
// Run: pio test -e native

#include <unity.h>
#include "ld2450/utils/schedule_time.h"

using namespace ld2450_time;

// --- parseHHMM validity ---
static void test_parse_valid(void) {
    int m;
    TEST_ASSERT_TRUE(parseHHMM("00:00", m)); TEST_ASSERT_EQUAL_INT(0, m);
    TEST_ASSERT_TRUE(parseHHMM("23:59", m)); TEST_ASSERT_EQUAL_INT(23*60+59, m);
    TEST_ASSERT_TRUE(parseHHMM("07:05", m)); TEST_ASSERT_EQUAL_INT(7*60+5, m);
    TEST_ASSERT_TRUE(parseHHMM("9:30", m));  TEST_ASSERT_EQUAL_INT(9*60+30, m);
    TEST_ASSERT_TRUE(parseHHMM("12:00", m)); TEST_ASSERT_EQUAL_INT(12*60, m);
}

static void test_parse_invalid(void) {
    int m;
    TEST_ASSERT_FALSE(parseHHMM("99:99", m));   // out of range
    TEST_ASSERT_FALSE(parseHHMM("24:00", m));   // hour 24 invalid
    TEST_ASSERT_FALSE(parseHHMM("12:60", m));   // minute 60 invalid
    TEST_ASSERT_FALSE(parseHHMM("", m));        // empty
    TEST_ASSERT_FALSE(parseHHMM("1234", m));    // no colon
    TEST_ASSERT_FALSE(parseHHMM("12:", m));     // nothing after colon
    TEST_ASSERT_FALSE(parseHHMM(":30", m));     // nothing before colon
    TEST_ASSERT_FALSE(parseHHMM("ab:cd", m));   // non-digit
    TEST_ASSERT_FALSE(parseHHMM("12:3x", m));   // trailing junk
    TEST_ASSERT_FALSE(parseHHMM("123:45", m));  // 3-digit hour
    TEST_ASSERT_FALSE(parseHHMM(nullptr, m));   // null
}

// --- scheduleDue ---
static void test_schedule_due(void) {
    TEST_ASSERT_TRUE(scheduleDue(7*60+30, "07:30"));
    TEST_ASSERT_FALSE(scheduleDue(7*60+31, "07:30"));
    TEST_ASSERT_FALSE(scheduleDue(0, ""));        // empty never due
    TEST_ASSERT_FALSE(scheduleDue(0, "99:99"));   // invalid never due
    TEST_ASSERT_TRUE(scheduleDue(0, "00:00"));    // midnight
}

// --- night window, normal (start<end) ---
static void test_night_normal(void) {
    bool night = true;
    // window 09:00-17:00
    TEST_ASSERT_TRUE(isNightNow(10*60, "09:00", "17:00", night)); TEST_ASSERT_TRUE(night);
    TEST_ASSERT_TRUE(isNightNow(8*60,  "09:00", "17:00", night)); TEST_ASSERT_FALSE(night);
    TEST_ASSERT_TRUE(isNightNow(17*60, "09:00", "17:00", night)); TEST_ASSERT_FALSE(night); // end exclusive
    TEST_ASSERT_TRUE(isNightNow(9*60,  "09:00", "17:00", night)); TEST_ASSERT_TRUE(night);  // start inclusive
}

// --- night window crossing midnight (start>end) ---
static void test_night_midnight(void) {
    bool night = false;
    // window 22:00 -> 06:00
    TEST_ASSERT_TRUE(isNightNow(23*60, "22:00", "06:00", night)); TEST_ASSERT_TRUE(night);
    TEST_ASSERT_TRUE(isNightNow(2*60,  "22:00", "06:00", night)); TEST_ASSERT_TRUE(night);
    TEST_ASSERT_TRUE(isNightNow(12*60, "22:00", "06:00", night)); TEST_ASSERT_FALSE(night);
    TEST_ASSERT_TRUE(isNightNow(6*60,  "22:00", "06:00", night)); TEST_ASSERT_FALSE(night); // end exclusive
    TEST_ASSERT_TRUE(isNightNow(22*60, "22:00", "06:00", night)); TEST_ASSERT_TRUE(night);  // start inclusive
}

// --- invalid/empty night bounds -> returns false, caller keeps day ---
static void test_night_invalid_bounds(void) {
    bool night = true;
    TEST_ASSERT_FALSE(isNightNow(12*60, "", "06:00", night));       // empty start
    TEST_ASSERT_FALSE(isNightNow(12*60, "22:00", "", night));       // empty end
    TEST_ASSERT_FALSE(isNightNow(12*60, "bad", "06:00", night));    // invalid start
    // equal bounds -> defined as no-night window
    TEST_ASSERT_TRUE(isNightNow(12*60, "08:00", "08:00", night));
    TEST_ASSERT_FALSE(night);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_parse_valid);
    RUN_TEST(test_parse_invalid);
    RUN_TEST(test_schedule_due);
    RUN_TEST(test_night_normal);
    RUN_TEST(test_night_midnight);
    RUN_TEST(test_night_invalid_bounds);
    return UNITY_END();
}
