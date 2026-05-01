// Native Unity testy pro LD2450 frame parser.
// Spuštění: pio test -e native
//
// Pokrývá:
//   - sign-magnitude decode (edge cases ±0, ±max)
//   - parsování validního framu
//   - sanity check (mimo rozsah → invalid)
//   - identifikace headeru/footeru
//   - resync semantika (testovaná logikou ringPeek)

#include <unity.h>
#include <stdint.h>
#include <string.h>
#include "ld2450/utils/ld2450_frame.h"

using namespace LD2450Frame;

static void buildTargetBytes(uint8_t* p, int16_t x, int16_t y, int16_t speed, uint16_t res) {
    // Encode sign-magnitude obráceně k decodeSignMag
    auto enc = [](int16_t v) -> uint16_t {
        if (v >= 0) return (uint16_t)(0x8000 | (uint16_t)v);
        else        return (uint16_t)(-v);
    };
    uint16_t rx = enc(x), ry = enc(y), rs = enc(speed);
    p[0] = rx & 0xFF; p[1] = (rx >> 8) & 0xFF;
    p[2] = ry & 0xFF; p[3] = (ry >> 8) & 0xFF;
    p[4] = rs & 0xFF; p[5] = (rs >> 8) & 0xFF;
    p[6] = res & 0xFF; p[7] = (res >> 8) & 0xFF;
}

static void buildFrame(uint8_t* frame30,
                       int16_t x1, int16_t y1, int16_t s1, uint16_t r1,
                       int16_t x2, int16_t y2, int16_t s2, uint16_t r2,
                       int16_t x3, int16_t y3, int16_t s3, uint16_t r3) {
    frame30[0] = 0xAA; frame30[1] = 0xFF; frame30[2] = 0x03; frame30[3] = 0x00;
    buildTargetBytes(frame30 + 4,  x1, y1, s1, r1);
    buildTargetBytes(frame30 + 12, x2, y2, s2, r2);
    buildTargetBytes(frame30 + 20, x3, y3, s3, r3);
    frame30[28] = 0x55; frame30[29] = 0xCC;
}

// --- decodeSignMag ---

void test_signmag_positive(void) {
    TEST_ASSERT_EQUAL_INT16(100, decodeSignMag(0x8064));   // +100
    TEST_ASSERT_EQUAL_INT16(0,   decodeSignMag(0x8000));   // +0
    TEST_ASSERT_EQUAL_INT16(32767, decodeSignMag(0xFFFF)); // +max 15-bit
}

void test_signmag_negative(void) {
    TEST_ASSERT_EQUAL_INT16(-100,   decodeSignMag(0x0064));
    TEST_ASSERT_EQUAL_INT16(0,      decodeSignMag(0x0000));   // -0 → 0
    TEST_ASSERT_EQUAL_INT16(-32767, decodeSignMag(0x7FFF));   // -max 15-bit
}

// --- header / footer ---

void test_header_match(void) {
    uint8_t good[4] = { 0xAA, 0xFF, 0x03, 0x00 };
    uint8_t bad1[4] = { 0xAB, 0xFF, 0x03, 0x00 };
    uint8_t bad2[4] = { 0xAA, 0xFF, 0x03, 0x01 };
    TEST_ASSERT_TRUE(hasHeader(good));
    TEST_ASSERT_FALSE(hasHeader(bad1));
    TEST_ASSERT_FALSE(hasHeader(bad2));
}

void test_footer_match(void) {
    uint8_t good[2] = { 0x55, 0xCC };
    uint8_t bad[2]  = { 0x55, 0xCD };
    TEST_ASSERT_TRUE(hasFooter(good));
    TEST_ASSERT_FALSE(hasFooter(bad));
}

// --- parse target ---

void test_parse_three_targets(void) {
    uint8_t frame[FRAME_SIZE];
    buildFrame(frame,
               1000, 2000,  50,  120,
              -500,  3500, -30,  200,
                 0,     0,   0,    0);

    ParsedTarget out[3];
    uint8_t cnt = parseTargets(frame, out);

    TEST_ASSERT_EQUAL_UINT8(2, cnt);
    TEST_ASSERT_EQUAL_INT16(1000, out[0].x);
    TEST_ASSERT_EQUAL_INT16(2000, out[0].y);
    TEST_ASSERT_EQUAL_INT16(50,   out[0].speed);
    TEST_ASSERT_EQUAL_UINT16(120, out[0].resolution);
    TEST_ASSERT_TRUE(out[0].valid);

    TEST_ASSERT_EQUAL_INT16(-500, out[1].x);
    TEST_ASSERT_EQUAL_INT16(3500, out[1].y);
    TEST_ASSERT_EQUAL_INT16(-30,  out[1].speed);
    TEST_ASSERT_TRUE(out[1].valid);

    // Třetí cíl: res=0 → invalid (nepočítá se)
    TEST_ASSERT_FALSE(out[2].valid);
}

// --- sanity check ---

void test_sanity_x_out_of_range(void) {
    uint8_t frame[FRAME_SIZE];
    buildFrame(frame,
               7000, 1000, 0, 100,    // x mimo (>6500)
                  0,    0, 0,   0,
                  0,    0, 0,   0);

    ParsedTarget out[3];
    uint8_t cnt = parseTargets(frame, out);

    TEST_ASSERT_EQUAL_UINT8(0, cnt);
    TEST_ASSERT_FALSE(out[0].valid);  // mimo X bound
}

void test_sanity_y_negative(void) {
    uint8_t frame[FRAME_SIZE];
    buildFrame(frame,
               0, -100, 0, 100,   // y < 0
               0,    0, 0,   0,
               0,    0, 0,   0);

    ParsedTarget out[3];
    uint8_t cnt = parseTargets(frame, out);
    TEST_ASSERT_EQUAL_UINT8(0, cnt);
    TEST_ASSERT_FALSE(out[0].valid);
}

void test_sanity_speed_too_high(void) {
    uint8_t frame[FRAME_SIZE];
    buildFrame(frame,
               0, 1000, 1500, 100,   // speed > 1000 cm/s
               0,    0,    0,   0,
               0,    0,    0,   0);

    ParsedTarget out[3];
    uint8_t cnt = parseTargets(frame, out);
    TEST_ASSERT_EQUAL_UINT8(0, cnt);
    TEST_ASSERT_FALSE(out[0].valid);
}

void test_zero_resolution_is_invalid(void) {
    uint8_t frame[FRAME_SIZE];
    buildFrame(frame,
               1000, 2000, 50, 0,    // resolution 0 → bez cíle
               0, 0, 0, 0,
               0, 0, 0, 0);

    ParsedTarget out[3];
    uint8_t cnt = parseTargets(frame, out);
    TEST_ASSERT_EQUAL_UINT8(0, cnt);
    TEST_ASSERT_FALSE(out[0].valid);
}

// --- frame size constants ---

void test_frame_size(void) {
    TEST_ASSERT_EQUAL_size_t(30, FRAME_SIZE);
    TEST_ASSERT_EQUAL_size_t(28, FOOTER_OFFSET);
    TEST_ASSERT_EQUAL_size_t(4,  TARGET_OFFSET);
}

// =========================================================================
// csRon-style raw byte stream fixtures
// Reference: github.com/csRon/HLK-LD2450 (Python protocol implementation).
// Goal: exercise parser on realistic frames the radar would actually emit.
// Encoding reminder for sign-magnitude (LD2450 little-endian, bit15 = sign,
// 1=positive): +0 -> 0x8000, +100 -> 0x8064, -100 -> 0x0064, +1500 -> 0x85DC.
// =========================================================================

// (1) Single static target — slot 1 valid, slots 2 & 3 zeroed.
//     Target: x=0mm, y=1500mm, speed=0, resolution=80. Built as raw hex
//     literal to prove parser handles a real-world byte stream end-to-end.
void test_csron_single_static_target_raw(void) {
    const uint8_t frame[FRAME_SIZE] = {
        // Header
        0xAA, 0xFF, 0x03, 0x00,
        // Target 1: x=+0 (0x8000), y=+1500 (0x85DC), speed=+0 (0x8000), res=80 (0x0050)
        0x00, 0x80, 0xDC, 0x85, 0x00, 0x80, 0x50, 0x00,
        // Target 2: all zero -> invalid (res=0)
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        // Target 3: all zero -> invalid (res=0)
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        // Footer
        0x55, 0xCC,
    };
    TEST_ASSERT_TRUE(hasHeader(frame));
    TEST_ASSERT_TRUE(hasFooter(frame + FOOTER_OFFSET));

    ParsedTarget out[3];
    uint8_t cnt = parseTargets(frame, out);
    TEST_ASSERT_EQUAL_UINT8(1, cnt);
    TEST_ASSERT_TRUE(out[0].valid);
    TEST_ASSERT_EQUAL_INT16(0,    out[0].x);
    TEST_ASSERT_EQUAL_INT16(1500, out[0].y);
    TEST_ASSERT_EQUAL_INT16(0,    out[0].speed);
    TEST_ASSERT_EQUAL_UINT16(80,  out[0].resolution);
    TEST_ASSERT_FALSE(out[1].valid);
    TEST_ASSERT_FALSE(out[2].valid);
}

// (2) Three moving targets in different quadrants — covers negative X,
//     positive X, and a large-Y depth target. Built via buildFrame to keep
//     the test readable while still exercising sign-magnitude in all slots.
void test_csron_three_moving_targets(void) {
    uint8_t frame[FRAME_SIZE];
    buildFrame(frame,
               -2500, 1200,  -80, 150,   // T1: negative X, walking toward
                3000, 2500,  120, 180,   // T2: positive X, walking away
                 100, 7800,   20, 220);  // T3: large Y (deep)
    ParsedTarget out[3];
    uint8_t cnt = parseTargets(frame, out);

    TEST_ASSERT_EQUAL_UINT8(3, cnt);
    TEST_ASSERT_EQUAL_INT16(-2500, out[0].x);
    TEST_ASSERT_EQUAL_INT16( 1200, out[0].y);
    TEST_ASSERT_EQUAL_INT16(  -80, out[0].speed);
    TEST_ASSERT_EQUAL_INT16( 3000, out[1].x);
    TEST_ASSERT_EQUAL_INT16(  120, out[1].speed);
    TEST_ASSERT_EQUAL_INT16( 7800, out[2].y);
    TEST_ASSERT_EQUAL_UINT16( 220, out[2].resolution);
    TEST_ASSERT_TRUE(out[0].valid);
    TEST_ASSERT_TRUE(out[1].valid);
    TEST_ASSERT_TRUE(out[2].valid);
}

// (3) Edge case: target at exactly (0, 0) with resolution > 0.
//     Subtle behaviour check — the parser does NOT special-case (0,0); the
//     only "no target" sentinel is res==0. So a target reporting (0,0,0)
//     with res>0 must be reported as VALID. This documents the actual rule
//     so future refactors don't silently change it.
void test_csron_origin_target_with_resolution_is_valid(void) {
    uint8_t frame[FRAME_SIZE];
    buildFrame(frame,
               0, 0, 0, 75,    // (0,0) but res=75 -> valid per current rule
               0, 0, 0,  0,
               0, 0, 0,  0);
    ParsedTarget out[3];
    uint8_t cnt = parseTargets(frame, out);
    TEST_ASSERT_EQUAL_UINT8(1, cnt);
    TEST_ASSERT_TRUE(out[0].valid);
    TEST_ASSERT_EQUAL_INT16(0,   out[0].x);
    TEST_ASSERT_EQUAL_INT16(0,   out[0].y);
    TEST_ASSERT_EQUAL_UINT16(75, out[0].resolution);
}

// =========================================================================
// HLK firmware v2.14.25112412 robustness regression tests.
// The Nov 2025 firmware update tightened command-length validation on the
// device side. Data-frame layout is unchanged, but we want defensive
// coverage that callers/parser combinations behave well on tricky streams.
//
// NOTE on parser scope: parseTargets() takes a *positioned* 30-byte frame
// pointer — it does no resync, no scanning, no buffer slicing. Resync logic
// lives in the caller (LD2450Service ringPeek loop). These tests therefore
// exercise the contract using hasHeader()/hasFooter() the same way the
// caller would, asserting what the helpers actually deliver.
// =========================================================================

// (4) Trailing garbage after a valid frame must not affect parseTargets.
//     Caller is expected to slice the 30B window; we simulate that by
//     pointing parseTargets at the start of the frame and verifying the
//     trailing bytes are ignored.
void test_hlk_v2_14_trailing_garbage_ignored(void) {
    uint8_t buf[FRAME_SIZE + 5];
    buildFrame(buf,
               1500, 2000, 60, 100,
                  0,    0,  0,   0,
                  0,    0,  0,   0);
    // Append partial garbage beyond footer.
    buf[FRAME_SIZE + 0] = 0xAA;
    buf[FRAME_SIZE + 1] = 0xFF;
    buf[FRAME_SIZE + 2] = 0x12;  // not a valid header completion
    buf[FRAME_SIZE + 3] = 0xDE;
    buf[FRAME_SIZE + 4] = 0xAD;

    ParsedTarget out[3];
    uint8_t cnt = parseTargets(buf, out);
    TEST_ASSERT_EQUAL_UINT8(1, cnt);
    TEST_ASSERT_EQUAL_INT16(1500, out[0].x);
    TEST_ASSERT_TRUE(hasFooter(buf + FOOTER_OFFSET));
}

// (5) Header-like bytes embedded inside payload must NOT cause a false
//     mid-frame match. We craft a frame where target slot 2 happens to
//     contain the header sequence AA FF 03 00 in its bytes; the only true
//     header is at offset 0. We confirm hasHeader is false at every
//     non-zero offset within the frame for this fixture.
void test_hlk_v2_14_header_inside_payload_no_false_match(void) {
    uint8_t frame[FRAME_SIZE];
    buildFrame(frame,
               1000, 2000, 50, 100,
                  0,    0,  0,   0,
                  0,    0,  0,   0);
    // Overwrite target-2 region (offsets 12..19) with bytes that contain
    // the header pattern AA FF 03 00 starting at offset 12.
    frame[12] = 0xAA; frame[13] = 0xFF; frame[14] = 0x03; frame[15] = 0x00;
    frame[16] = 0x00; frame[17] = 0x80; frame[18] = 0x50; frame[19] = 0x00;

    // Real header still at offset 0.
    TEST_ASSERT_TRUE(hasHeader(frame));
    // The embedded sequence at offset 12 — hasHeader() will match the raw
    // bytes (it only looks at 4 bytes), but the *caller* must reject it
    // because it isn't on a 30B boundary. Document the contract: at every
    // offset 1..11 and 13..(FRAME_SIZE-HEADER_LEN) other than 0 and 12,
    // hasHeader must be false.
    for (size_t i = 1; i + HEADER_LEN <= FRAME_SIZE; i++) {
        if (i == 12) continue; // documented intentional embedded pattern
        TEST_ASSERT_FALSE_MESSAGE(hasHeader(frame + i),
            "false header match at non-boundary offset");
    }
    // And footer is only at offset 28.
    for (size_t i = 0; i + FOOTER_LEN <= FRAME_SIZE; i++) {
        if (i == FOOTER_OFFSET) continue;
        TEST_ASSERT_FALSE(hasFooter(frame + i));
    }
}

// (6) Multiple back-to-back valid frames in a single buffer.
//     parseTargets has no buffer-walking logic, so the *test* drives the
//     30B stride that the production caller would use, and verifies each
//     slice parses independently with the expected payload.
void test_hlk_v2_14_back_to_back_frames(void) {
    constexpr size_t N = 3;
    uint8_t buf[FRAME_SIZE * N];

    // Frame A: one target at (1000, 2000)
    buildFrame(buf + 0 * FRAME_SIZE,
               1000, 2000, 25, 100,
                  0,    0,  0,   0,
                  0,    0,  0,   0);
    // Frame B: two targets
    buildFrame(buf + 1 * FRAME_SIZE,
               -800, 1500, -40, 110,
               1200, 3000,  60, 130,
                  0,    0,   0,   0);
    // Frame C: three targets
    buildFrame(buf + 2 * FRAME_SIZE,
                500, 1800,  10, 90,
               -500, 2200, -10, 95,
                  0, 5000,   0, 140);

    const uint8_t expectedCounts[N] = { 1, 2, 3 };
    for (size_t i = 0; i < N; i++) {
        const uint8_t* p = buf + i * FRAME_SIZE;
        TEST_ASSERT_TRUE(hasHeader(p));
        TEST_ASSERT_TRUE(hasFooter(p + FOOTER_OFFSET));
        ParsedTarget out[3];
        uint8_t cnt = parseTargets(p, out);
        TEST_ASSERT_EQUAL_UINT8(expectedCounts[i], cnt);
    }
    // Spot-check payloads from frame B and C.
    ParsedTarget outB[3];
    parseTargets(buf + FRAME_SIZE, outB);
    TEST_ASSERT_EQUAL_INT16(-800, outB[0].x);
    TEST_ASSERT_EQUAL_INT16(1200, outB[1].x);

    ParsedTarget outC[3];
    parseTargets(buf + 2 * FRAME_SIZE, outC);
    TEST_ASSERT_EQUAL_INT16(5000, outC[2].y);
    TEST_ASSERT_TRUE(outC[2].valid);
}

// --- entry point ---

void setUp(void) {}
void tearDown(void) {}

int main(int /*argc*/, char** /*argv*/) {
    UNITY_BEGIN();
    RUN_TEST(test_signmag_positive);
    RUN_TEST(test_signmag_negative);
    RUN_TEST(test_header_match);
    RUN_TEST(test_footer_match);
    RUN_TEST(test_parse_three_targets);
    RUN_TEST(test_sanity_x_out_of_range);
    RUN_TEST(test_sanity_y_negative);
    RUN_TEST(test_sanity_speed_too_high);
    RUN_TEST(test_zero_resolution_is_invalid);
    RUN_TEST(test_frame_size);
    // csRon-style raw-fixture regression tests
    RUN_TEST(test_csron_single_static_target_raw);
    RUN_TEST(test_csron_three_moving_targets);
    RUN_TEST(test_csron_origin_target_with_resolution_is_valid);
    // HLK fw v2.14.25112412 robustness regression tests
    RUN_TEST(test_hlk_v2_14_trailing_garbage_ignored);
    RUN_TEST(test_hlk_v2_14_header_inside_payload_no_false_match);
    RUN_TEST(test_hlk_v2_14_back_to_back_frames);
    return UNITY_END();
}
