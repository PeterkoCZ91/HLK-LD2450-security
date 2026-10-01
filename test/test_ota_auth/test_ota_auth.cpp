// Native Unity tests for the SEC-01 Web OTA authorization gate.
// Spuštění: pio test -e native
//
// These lock the fail-closed invariant: the Update library must never be written
// for an OTA upload whose first chunk did not authenticate, and only one OTA may
// be active at a time. The gate helpers are pure so they run on the host without
// AsyncWebServer/Update.

#include <unity.h>
#include "ld2450/utils/ota_upload_gate.h"

using namespace ld2450;

void setUp(void) {}
void tearDown(void) {}

// --- otaMayBegin: fail-closed first-chunk decision ---

void test_begin_requires_auth(void) {
    // Unauthenticated first chunk must never begin, regardless of active state.
    TEST_ASSERT_FALSE(otaMayBegin(false, false));
    TEST_ASSERT_FALSE(otaMayBegin(false, true));
}

void test_begin_rejects_when_busy(void) {
    // Authenticated but another OTA already owns Update → reject (one at a time).
    TEST_ASSERT_FALSE(otaMayBegin(true, true));
}

void test_begin_allows_authenticated_and_idle(void) {
    // Only the authenticated + idle case may start writing firmware.
    TEST_ASSERT_TRUE(otaMayBegin(true, false));
}

// --- otaMayWrite: subsequent-chunk ownership gate ---

void test_write_requires_ownership_token(void) {
    // Without an ownership token (never granted to unauthenticated sessions) no
    // data/final chunk may touch Update.
    TEST_ASSERT_FALSE(otaMayWrite(false));
    TEST_ASSERT_TRUE(otaMayWrite(true));
}

// --- End-to-end chunk sequence: unauthorized upload never writes ---

// Simulate the upload callback's decision path for a multi-chunk upload and count
// how many chunks would have written to Update.
static int simulateWrites(bool authenticated, bool otaActiveAtStart, int chunks) {
    bool otaActive = otaActiveAtStart;
    bool ownsToken = false;  // per-request ownership token
    int writes = 0;
    for (int i = 0; i < chunks; i++) {
        bool first = (i == 0);
        if (first) {
            if (!otaMayBegin(authenticated, otaActive)) {
                // Rejected: no Update.begin(), no token granted.
                continue;
            }
            ownsToken = true;   // Update.begin() succeeded, ownership claimed
            otaActive = true;
        }
        if (!otaMayWrite(ownsToken)) continue;  // gate every chunk on ownership
        writes++;               // Update.write()/end() would run here
    }
    return writes;
}

void test_unauthorized_upload_writes_nothing(void) {
    // A 5-chunk unauthenticated upload must produce zero Update writes.
    TEST_ASSERT_EQUAL_INT(0, simulateWrites(false, false, 5));
}

void test_busy_upload_writes_nothing(void) {
    // Authenticated upload while another OTA is active must produce zero writes.
    TEST_ASSERT_EQUAL_INT(0, simulateWrites(true, true, 5));
}

void test_authorized_upload_writes_all_chunks(void) {
    // Authenticated, idle: every chunk writes.
    TEST_ASSERT_EQUAL_INT(5, simulateWrites(true, false, 5));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_begin_requires_auth);
    RUN_TEST(test_begin_rejects_when_busy);
    RUN_TEST(test_begin_allows_authenticated_and_idle);
    RUN_TEST(test_write_requires_ownership_token);
    RUN_TEST(test_unauthorized_upload_writes_nothing);
    RUN_TEST(test_busy_upload_writes_nothing);
    RUN_TEST(test_authorized_upload_writes_all_chunks);
    return UNITY_END();
}
