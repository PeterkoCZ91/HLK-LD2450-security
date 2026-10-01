#include <unity.h>
#include <math.h>
#include "ld2450/utils/EKF2D.h"

void setUp(void) {}
void tearDown(void) {}

static void test_nan_measurement_ignored(void) {
    EKF2D f;
    f.update(100, 200, 1000);
    f.update(110, 210, 1100);
    f.update(NAN, 5, 1200);
    TEST_ASSERT_TRUE(isfinite(f.getX()));
    f.update(120, 220, 1300);
    TEST_ASSERT_TRUE(isfinite(f.getX()) && isfinite(f.getVY()));
}

static void test_millis_wrap(void) {
    EKF2D f;
    f.update(100, 100, 0xFFFFFF00UL);
    f.update(100, 100, 0x00000100UL);  // wrap; dt must not explode
    TEST_ASSERT_TRUE(isfinite(f.getX()));
    TEST_ASSERT_FLOAT_WITHIN(50.0f, 100.0f, f.getX());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_nan_measurement_ignored);
    RUN_TEST(test_millis_wrap);
    return UNITY_END();
}
