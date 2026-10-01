// Native Unity tests for config backup schema validation (CFG-03 / SEC-07)
// and the shared CRC32 (MQTT-01). Run: pio test -e native

#include <unity.h>
#include "ld2450/utils/config_schema.h"
#include "ld2450/utils/crc32.h"

using namespace ld2450_cfg;

static void test_version_supported(void) {
    TEST_ASSERT_TRUE(versionSupported(1));
    TEST_ASSERT_TRUE(versionSupported(SCHEMA_VERSION));
    TEST_ASSERT_FALSE(versionSupported(0));
    TEST_ASSERT_FALSE(versionSupported(SCHEMA_VERSION + 1)); // future = reject
    TEST_ASSERT_FALSE(versionSupported(-3));
}

static void test_in_range(void) {
    TEST_ASSERT_TRUE(inRange(0, R_ZONE_X.lo, R_ZONE_X.hi));
    TEST_ASSERT_TRUE(inRange(-8000, R_ZONE_X.lo, R_ZONE_X.hi));
    TEST_ASSERT_TRUE(inRange(8000, R_ZONE_X.lo, R_ZONE_X.hi));
    TEST_ASSERT_FALSE(inRange(8001, R_ZONE_X.lo, R_ZONE_X.hi));
    TEST_ASSERT_FALSE(inRange(-8001, R_ZONE_X.lo, R_ZONE_X.hi));
}

static void test_thresholds(void) {
    TEST_ASSERT_TRUE(inRange(100, R_MIN_RES.lo, R_MIN_RES.hi));
    TEST_ASSERT_FALSE(inRange(3000, R_MIN_RES.lo, R_MIN_RES.hi));
    TEST_ASSERT_TRUE(inRange(120000, R_GHOST_TIMEOUT.lo, R_GHOST_TIMEOUT.hi));
    TEST_ASSERT_FALSE(inRange(700000, R_GHOST_TIMEOUT.lo, R_GHOST_TIMEOUT.hi));
    TEST_ASSERT_TRUE(inRange(30000, R_PERSIST_MS.lo, R_PERSIST_MS.hi));
    TEST_ASSERT_FALSE(inRange(30001, R_PERSIST_MS.lo, R_PERSIST_MS.hi));
}

static void test_rotation(void) {
    TEST_ASSERT_TRUE(rotationValid(0));
    TEST_ASSERT_TRUE(rotationValid(90));
    TEST_ASSERT_TRUE(rotationValid(180));
    TEST_ASSERT_TRUE(rotationValid(270));
    TEST_ASSERT_FALSE(rotationValid(45));
    TEST_ASSERT_FALSE(rotationValid(360));
    TEST_ASSERT_FALSE(rotationValid(-90));
}

// --- CRC32 (MQTT-01 durability metadata) ---
static void test_crc32_known(void) {
    // "123456789" -> 0xCBF43926 (canonical CRC-32/IEEE check value)
    const uint8_t d[] = {'1','2','3','4','5','6','7','8','9'};
    uint32_t c = ld2450_crc::crc32(d, sizeof(d)) ^ 0xFFFFFFFFu;
    TEST_ASSERT_EQUAL_HEX32(0xCBF43926u, c);
}

static void test_crc32_detects_flip(void) {
    uint8_t a[] = {1,2,3,4,5,6,7,8};
    uint8_t b[] = {1,2,3,4,5,6,7,9}; // one byte changed
    TEST_ASSERT_NOT_EQUAL(ld2450_crc::crc32(a, sizeof(a)),
                          ld2450_crc::crc32(b, sizeof(b)));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_version_supported);
    RUN_TEST(test_in_range);
    RUN_TEST(test_thresholds);
    RUN_TEST(test_rotation);
    RUN_TEST(test_crc32_known);
    RUN_TEST(test_crc32_detects_flip);
    return UNITY_END();
}
