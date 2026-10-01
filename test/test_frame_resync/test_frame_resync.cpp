// Native Unity tests for the production frame resynchronisation strategy (TST-02).
// Run: pio test -e native
//
// The production RX path (LD2450Service::parseFrame) resynchronises a byte
// stream: scan for the AA FF 03 00 header, verify the 55 CC footer at offset 28,
// parse on match (consume 30 bytes), else skip 1 byte and keep searching. These
// tests replicate that exact strategy over a linear buffer and feed it the cases
// the old parser tests did not: leading noise, back-to-back frames, truncation,
// and a corrupt footer that must not desync the stream.

#include <unity.h>
#include <vector>
#include <cstring>
#include "ld2450/utils/ld2450_frame.h"

using namespace LD2450Frame;

// Mirror of the production resync loop over a linear buffer. Returns the number
// of complete valid frames extracted and leaves `consumed` at the read cursor.
static int resync(const std::vector<uint8_t>& buf, int& consumed) {
    int frames = 0;
    size_t tail = 0;
    while (buf.size() - tail >= FRAME_SIZE) {
        const uint8_t* p = &buf[tail];
        if (hasHeader(p)) {
            if (hasFooter(p + FOOTER_OFFSET)) {
                ParsedTarget pt[3];
                parseTargets(p, pt);
                frames++;
                tail += FRAME_SIZE;   // consume whole frame
            } else {
                tail += 1;            // header ok, footer bad -> skip 1, keep searching
            }
        } else {
            tail += 1;                // not a header
        }
    }
    consumed = (int)tail;
    return frames;
}

static void encTarget(uint8_t* p, int16_t x, int16_t y, int16_t s, uint16_t r) {
    auto enc = [](int16_t v) -> uint16_t {
        return (v >= 0) ? (uint16_t)(0x8000 | (uint16_t)v) : (uint16_t)(-v);
    };
    uint16_t rx = enc(x), ry = enc(y), rs = enc(s);
    p[0]=rx&0xFF; p[1]=rx>>8; p[2]=ry&0xFF; p[3]=ry>>8;
    p[4]=rs&0xFF; p[5]=rs>>8; p[6]=r&0xFF; p[7]=r>>8;
}

static std::vector<uint8_t> makeFrame() {
    std::vector<uint8_t> f(FRAME_SIZE, 0);
    f[0]=0xAA; f[1]=0xFF; f[2]=0x03; f[3]=0x00;
    encTarget(&f[4],  100, 2000, 10, 50);
    encTarget(&f[12], 0, 0, 0, 0);
    encTarget(&f[20], 0, 0, 0, 0);
    f[28]=0x55; f[29]=0xCC;
    return f;
}

// --- 1: clean single frame ---
static void test_single(void) {
    auto f = makeFrame();
    int consumed = 0;
    TEST_ASSERT_EQUAL_INT(1, resync(f, consumed));
    TEST_ASSERT_EQUAL_INT(FRAME_SIZE, consumed);
}

// --- 2: leading noise before a frame ---
static void test_noise_prefix(void) {
    std::vector<uint8_t> buf = {0x11,0x22,0xAA,0xFF,0x00,0x99}; // fake-ish junk
    auto f = makeFrame();
    buf.insert(buf.end(), f.begin(), f.end());
    int consumed = 0;
    TEST_ASSERT_EQUAL_INT(1, resync(buf, consumed));
}

// --- 3: back-to-back frames ---
static void test_back_to_back(void) {
    auto a = makeFrame(); auto b = makeFrame();
    std::vector<uint8_t> buf(a);
    buf.insert(buf.end(), b.begin(), b.end());
    int consumed = 0;
    TEST_ASSERT_EQUAL_INT(2, resync(buf, consumed));
    TEST_ASSERT_EQUAL_INT(2 * (int)FRAME_SIZE, consumed);
}

// --- 4: truncated trailing frame is not falsely parsed ---
static void test_truncated(void) {
    auto a = makeFrame(); auto b = makeFrame();
    b.resize(FRAME_SIZE - 5); // cut the tail
    std::vector<uint8_t> buf(a);
    buf.insert(buf.end(), b.begin(), b.end());
    int consumed = 0;
    // Only the first, complete frame counts; the partial one stays buffered.
    TEST_ASSERT_EQUAL_INT(1, resync(buf, consumed));
}

// --- 5: corrupt footer must not desync following frame ---
static void test_corrupt_footer_recovers(void) {
    auto bad = makeFrame(); bad[29] = 0x00;   // break footer
    auto good = makeFrame();
    std::vector<uint8_t> buf(bad);
    buf.insert(buf.end(), good.begin(), good.end());
    int consumed = 0;
    // The bad frame's header is skipped byte-by-byte; the good frame is found.
    TEST_ASSERT_EQUAL_INT(1, resync(buf, consumed));
}

// --- 6: header-like bytes inside noise don't cause a false parse ---
static void test_false_header(void) {
    std::vector<uint8_t> buf(FRAME_SIZE + 10, 0x00);
    buf[0]=0xAA; buf[1]=0xFF; buf[2]=0x03; buf[3]=0x00; // header but zero footer
    int consumed = 0;
    TEST_ASSERT_EQUAL_INT(0, resync(buf, consumed));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_single);
    RUN_TEST(test_noise_prefix);
    RUN_TEST(test_back_to_back);
    RUN_TEST(test_truncated);
    RUN_TEST(test_corrupt_footer_recovers);
    RUN_TEST(test_false_header);
    return UNITY_END();
}
