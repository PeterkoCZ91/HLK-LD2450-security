// Native Unity tests for SEC-07 upload guards and typed import models.
// Spusteni: pio test -e native
#include <unity.h>
#include <cstdlib>
#include <cstring>
#include "ld2450/utils/upload_guard.h"

using namespace ld2450;

void setUp(void) {}
void tearDown(void) {}

static const uint8_t* U(const char* s) { return reinterpret_cast<const uint8_t*>(s); }

// --- admission ---
void test_unauth_never_admitted_and_learns_nothing(void) {
    // Unauthenticated: always Unauthorized, regardless of busy/length.
    TEST_ASSERT_EQUAL((int)UploadVerdict::Unauthorized, (int)uploadAdmit(false, false, 100, 4096));
    TEST_ASSERT_EQUAL((int)UploadVerdict::Unauthorized, (int)uploadAdmit(false, true, 0, 4096));
    TEST_ASSERT_EQUAL((int)UploadVerdict::Unauthorized, (int)uploadAdmit(false, false, 999999, 4096));
    TEST_ASSERT_EQUAL(401, uploadHttpStatus(UploadVerdict::Unauthorized));
}
void test_length_rules(void) {
    TEST_ASSERT_EQUAL((int)UploadVerdict::NoLength, (int)uploadAdmit(true, false, 0, 4096));
    TEST_ASSERT_EQUAL((int)UploadVerdict::TooLarge, (int)uploadAdmit(true, false, 4097, 4096));
    TEST_ASSERT_EQUAL((int)UploadVerdict::Ok, (int)uploadAdmit(true, false, 4096, 4096));
    TEST_ASSERT_EQUAL((int)UploadVerdict::Ok, (int)uploadAdmit(true, false, 1, 4096));
    TEST_ASSERT_EQUAL(413, uploadHttpStatus(UploadVerdict::TooLarge));
    TEST_ASSERT_EQUAL(411, uploadHttpStatus(UploadVerdict::NoLength));
}
void test_busy_rejected_when_authed(void) {
    TEST_ASSERT_EQUAL((int)UploadVerdict::Busy, (int)uploadAdmit(true, true, 100, 4096));
    TEST_ASSERT_EQUAL(503, uploadHttpStatus(UploadVerdict::Busy));
}

// --- single-session slot ---
void test_slot_single_owner_and_release(void) {
    UploadSlot s; int a, b;
    TEST_ASSERT_TRUE(s.tryAcquire(1000, 15000, &a));
    TEST_ASSERT_FALSE(s.tryAcquire(1100, 15000, &b));   // concurrent refused
    s.release(&b);                                        // non-owner cannot release
    TEST_ASSERT_TRUE(s.busy(1200, 15000));
    s.release(&a);
    TEST_ASSERT_TRUE(s.tryAcquire(1300, 15000, &b));
}
void test_slot_stale_reclaim_and_touch(void) {
    UploadSlot s; int a, b;
    TEST_ASSERT_TRUE(s.tryAcquire(0, 15000, &a));
    s.touch(10000, &a);
    TEST_ASSERT_FALSE(s.tryAcquire(20000, 15000, &b));  // touched at 10000, still live
    TEST_ASSERT_TRUE(s.tryAcquire(26000, 15000, &b));   // stale -> reclaimed
    s.release(&a);                                        // old owner cannot free new owner's slot
    TEST_ASSERT_TRUE(s.busy(26001, 15000));
}
void test_slot_millis_wrap(void) {
    UploadSlot s; int a, b;
    TEST_ASSERT_TRUE(s.tryAcquire(0xFFFFFF00u, 15000, &a));
    TEST_ASSERT_FALSE(s.tryAcquire(0x00000100u, 15000, &b));  // 512 ms later across wrap
}

// --- buffer ---
void test_buf_append_and_complete(void) {
    UploadBuf* b = UploadBuf::create(16, 10);
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_TRUE(b->append(U("hello"), 5));
    TEST_ASSERT_FALSE(b->complete());               // truncated: 5 of 10
    TEST_ASSERT_TRUE(b->append(U("world"), 5));
    TEST_ASSERT_TRUE(b->complete());
    TEST_ASSERT_EQUAL_STRING("helloworld", b->c_str());
    std::free(b);
}
void test_buf_overflow_latches(void) {
    UploadBuf* b = UploadBuf::create(8);
    TEST_ASSERT_TRUE(b->append(U("12345"), 5));
    TEST_ASSERT_FALSE(b->append(U("6789"), 4));     // 9 > 8
    TEST_ASSERT_TRUE(b->overflow);
    TEST_ASSERT_FALSE(b->append(U("x"), 1));        // latched: later small chunks refused
    TEST_ASSERT_FALSE(b->complete());
    TEST_ASSERT_EQUAL_UINT(5, b->len);              // nothing beyond cap stored
    std::free(b);
}
void test_buf_exact_cap_and_empty(void) {
    UploadBuf* b = UploadBuf::create(4);
    TEST_ASSERT_FALSE(b->complete());               // empty is not complete
    TEST_ASSERT_TRUE(b->append(U("abcd"), 4));
    TEST_ASSERT_TRUE(b->complete());
    TEST_ASSERT_FALSE(b->append(U("e"), 1));
    std::free(b);
}
void test_buf_huge_len_no_wrap(void) {
    UploadBuf* b = UploadBuf::create(8);
    TEST_ASSERT_FALSE(b->append(U("a"), (size_t)-1));  // size_t overflow guard
    TEST_ASSERT_TRUE(b->overflow);
    std::free(b);
}

// --- string validators ---
void test_port_string(void) {
    TEST_ASSERT_TRUE(portStringValid("1883"));
    TEST_ASSERT_TRUE(portStringValid("65535"));
    TEST_ASSERT_FALSE(portStringValid("0"));
    TEST_ASSERT_FALSE(portStringValid("65536"));
    TEST_ASSERT_FALSE(portStringValid(""));
    TEST_ASSERT_FALSE(portStringValid("12a"));
    TEST_ASSERT_FALSE(portStringValid("123456"));
    TEST_ASSERT_FALSE(portStringValid("-1"));
}
void test_ctl_chars(void) {
    TEST_ASSERT_TRUE(printableNoCtl("broker.local"));
    TEST_ASSERT_FALSE(printableNoCtl("a\nb"));
    TEST_ASSERT_FALSE(printableNoCtl("a\x7f"));
}

// --- ConfigImportModel ---
void test_model_empty_valid(void) {
    ConfigImportModel m;
    TEST_ASSERT_NULL(m.validate());
}
void test_model_valid_full(void) {
    ConfigImportModel m;
    m.xmin.set(-3000); m.xmax.set(3000); m.ymin.set(0); m.ymax.set(6000);
    m.mapRotation.set(90); m.loiterMs.set(60000); m.antimaskTime.set(30);
    m.mqttServer.set("broker.local"); m.mqttPort.set("1883");
    m.mqttUser.set("u"); m.mqttPass.set("p"); m.tgChat.set("12345");
    TEST_ASSERT_NULL(m.validate());
}
void test_model_out_of_range(void) {
    ConfigImportModel m; m.xmin.set(-8001);
    TEST_ASSERT_EQUAL_STRING("xmin", m.validate());
    ConfigImportModel n; n.heartbeatMs.set(86400001L);
    TEST_ASSERT_EQUAL_STRING("heartbeat_ms", n.validate());
    ConfigImportModel o; o.persistenceMs.set(-1);
    TEST_ASSERT_EQUAL_STRING("persistence_ms", o.validate());
}
void test_model_bad_rotation(void) {
    ConfigImportModel m; m.mapRotation.set(45);
    TEST_ASSERT_EQUAL_STRING("map_rotation", m.validate());
}
void test_model_type_error_wins(void) {
    ConfigImportModel m; m.typeError = "loiter_ms";
    TEST_ASSERT_EQUAL_STRING("loiter_ms", m.validate());
}
void test_model_string_limits(void) {
    char longSrv[MQTT_SERVER_MAX + 20]; std::memset(longSrv, 'a', sizeof(longSrv) - 1); longSrv[sizeof(longSrv) - 1] = 0;
    ConfigImportModel m; m.mqttServer.set(longSrv);
    TEST_ASSERT_TRUE(m.mqttServer.tooLong);
    TEST_ASSERT_EQUAL_STRING("mqtt.server", m.validate());
    // exactly at the limit is fine
    char okSrv[MQTT_SERVER_MAX + 1]; std::memset(okSrv, 'a', MQTT_SERVER_MAX); okSrv[MQTT_SERVER_MAX] = 0;
    ConfigImportModel n; n.mqttServer.set(okSrv);
    TEST_ASSERT_NULL(n.validate());
    ConfigImportModel p; p.mqttPort.set("99999");
    TEST_ASSERT_EQUAL_STRING("mqtt.port", p.validate());
    ConfigImportModel q; q.tgChat.set("bad\nchat");
    TEST_ASSERT_EQUAL_STRING("telegram.chat_id", q.validate());
    ConfigImportModel r; char pw[MQTT_PASS_MAX + 5]; std::memset(pw, 'p', sizeof(pw) - 1); pw[sizeof(pw) - 1] = 0;
    r.mqttPass.set(pw);
    TEST_ASSERT_EQUAL_STRING("mqtt.pass", r.validate());
}
void test_model_atomic_one_bad_field_rejects_all(void) {
    // Many good fields + a single bad one -> whole model invalid (route applies nothing).
    ConfigImportModel m;
    m.xmin.set(-1000); m.xmax.set(1000); m.mqttServer.set("ok");
    m.exitDelayMs.set(600001L);
    TEST_ASSERT_EQUAL_STRING("exit_delay_ms", m.validate());
}

// --- RegionFilterModel ---
void test_region_valid(void) {
    RegionFilterModel m; m.mode = 1; m.coords[0] = -10000; m.coords[3] = 10000;
    TEST_ASSERT_NULL(m.validate());
}
void test_region_invalid(void) {
    RegionFilterModel m;                      // default mode -1
    TEST_ASSERT_EQUAL_STRING("mode", m.validate());
    m.mode = 3;
    TEST_ASSERT_EQUAL_STRING("mode", m.validate());
    m.mode = 1; m.coords[5] = 10001;
    TEST_ASSERT_EQUAL_STRING("coord", m.validate());
    m.coords[5] = 0; m.coords[6] = -10001;
    TEST_ASSERT_EQUAL_STRING("coord", m.validate());
    m.coords[6] = 0; m.tooManyZones = true;
    TEST_ASSERT_EQUAL_STRING("zones", m.validate());
}
void test_region_no_int16_wrap(void) {
    // 70000 would wrap to 4464 if narrowed to int16 before validation.
    RegionFilterModel m; m.mode = 1; m.coords[0] = 70000;
    TEST_ASSERT_EQUAL_STRING("coord", m.validate());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_unauth_never_admitted_and_learns_nothing);
    RUN_TEST(test_length_rules);
    RUN_TEST(test_busy_rejected_when_authed);
    RUN_TEST(test_slot_single_owner_and_release);
    RUN_TEST(test_slot_stale_reclaim_and_touch);
    RUN_TEST(test_slot_millis_wrap);
    RUN_TEST(test_buf_append_and_complete);
    RUN_TEST(test_buf_overflow_latches);
    RUN_TEST(test_buf_exact_cap_and_empty);
    RUN_TEST(test_buf_huge_len_no_wrap);
    RUN_TEST(test_port_string);
    RUN_TEST(test_ctl_chars);
    RUN_TEST(test_model_empty_valid);
    RUN_TEST(test_model_valid_full);
    RUN_TEST(test_model_out_of_range);
    RUN_TEST(test_model_bad_rotation);
    RUN_TEST(test_model_type_error_wins);
    RUN_TEST(test_model_string_limits);
    RUN_TEST(test_model_atomic_one_bad_field_rejects_all);
    RUN_TEST(test_region_valid);
    RUN_TEST(test_region_invalid);
    RUN_TEST(test_region_no_int16_wrap);
    return UNITY_END();
}
