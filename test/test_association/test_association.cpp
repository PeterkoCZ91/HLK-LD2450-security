// Native Unity tests for TargetAssociation optimal partial matching (ALG-01).
// Run: pio test -e native
//
// Focus: nTrk vs nDet mismatches where the previous implementation dropped
// the last tracks regardless of distance, plus crossing/gating/equal-cost.

#include <unity.h>
#include <stdint.h>
#include "ld2450/utils/TargetAssociation.h"

// Helpers ------------------------------------------------------------------

static void run(const float dx[3], const float dy[3], const bool dv[3],
                const float tx[3], const float ty[3], const bool tv[3],
                uint8_t out[3]) {
    associateTargets(dx, dy, dv, tx, ty, tv, out);
}

// --- 1: single detection, single track, within gate -> matches ---
static void test_1det_1trk_match(void) {
    float dx[3] = {100, 0, 0}, dy[3] = {100, 0, 0};
    bool dv[3] = {true, false, false};
    float tx[3] = {120, 0, 0}, ty[3] = {90, 0, 0};
    bool tv[3] = {true, false, false};
    uint8_t m[3];
    run(dx, dy, dv, tx, ty, tv, m);
    TEST_ASSERT_EQUAL_UINT8(0, m[0]);
}

// --- 2: 2 detections, 3 tracks -> best 2 tracks kept, worst dropped ---
// Regression for ALG-01: previously tracks 0,1 were always chosen and
// track 2 dropped even if track 2 was the closest.
static void test_2det_3trk_best_subset(void) {
    // Detections near track2 and track0; track1 is far from both.
    float dx[3] = {0,    5000, 0};
    float dy[3] = {0,    0,    0};
    bool  dv[3] = {true, true, false};
    // trk0 close to det0, trk1 far away, trk2 close to det1.
    float tx[3] = {50,   9000, 4950};
    float ty[3] = {0,    9000, 0};
    bool  tv[3] = {true, true, true};
    uint8_t m[3];
    run(dx, dy, dv, tx, ty, tv, m);
    TEST_ASSERT_EQUAL_UINT8(0, m[0]);        // trk0 -> det0
    TEST_ASSERT_EQUAL_UINT8(NO_MATCH, m[1]); // trk1 dropped (far)
    TEST_ASSERT_EQUAL_UINT8(1, m[2]);        // trk2 -> det1 (must not be dropped)
}

// --- 3: 3 detections, 2 tracks -> both tracks matched, surplus is new ---
static void test_3det_2trk_surplus_new(void) {
    float dx[3] = {0,    1000, 5000};
    float dy[3] = {0,    0,    0};
    bool  dv[3] = {true, true, true};
    float tx[3] = {10,   1010, 0};
    float ty[3] = {0,    0,    0};
    bool  tv[3] = {true, true, false};
    uint8_t m[3];
    run(dx, dy, dv, tx, ty, tv, m);
    TEST_ASSERT_EQUAL_UINT8(0, m[0]);
    TEST_ASSERT_EQUAL_UINT8(1, m[1]);
    // surplus det2 assigned to the free slot (slot 2)
    TEST_ASSERT_EQUAL_UINT8(2, m[2]);
}

// --- 4: crossing targets keep nearest identity ---
static void test_crossing(void) {
    float dx[3] = {-100, 100, 0};
    float dy[3] = {0,    0,   0};
    bool  dv[3] = {true, true, false};
    float tx[3] = {110,  -110, 0}; // trk0 near det1, trk1 near det0
    float ty[3] = {0,    0,    0};
    bool  tv[3] = {true, true, false};
    uint8_t m[3];
    run(dx, dy, dv, tx, ty, tv, m);
    TEST_ASSERT_EQUAL_UINT8(1, m[0]);
    TEST_ASSERT_EQUAL_UINT8(0, m[1]);
}

// --- 5: detection out of gate -> track dropped, det becomes new ---
static void test_out_of_gate(void) {
    float dx[3] = {9000, 0, 0};
    float dy[3] = {0,    0, 0};
    bool  dv[3] = {true, false, false};
    float tx[3] = {0,    0, 0}; // 9000mm away > 2000 gate
    float ty[3] = {0,    0, 0};
    bool  tv[3] = {true, false, false};
    uint8_t m[3];
    run(dx, dy, dv, tx, ty, tv, m);
    // trk0 cannot match (gate) -> its slot stays NO_MATCH.
    TEST_ASSERT_EQUAL_UINT8(NO_MATCH, m[0]);
    // det0 placed as new target in a free non-valid slot (1 or 2).
    bool placed = (m[1] == 0) || (m[2] == 0);
    TEST_ASSERT_TRUE(placed);
}

// --- 6: no detections -> all NO_MATCH ---
static void test_no_det(void) {
    float dx[3] = {0,0,0}, dy[3] = {0,0,0};
    bool  dv[3] = {false,false,false};
    float tx[3] = {0,0,0}, ty[3] = {0,0,0};
    bool  tv[3] = {true,false,false};
    uint8_t m[3];
    run(dx, dy, dv, tx, ty, tv, m);
    TEST_ASSERT_EQUAL_UINT8(NO_MATCH, m[0]);
    TEST_ASSERT_EQUAL_UINT8(NO_MATCH, m[1]);
    TEST_ASSERT_EQUAL_UINT8(NO_MATCH, m[2]);
}

// --- 7: equal-cost tie is resolved deterministically without duplicate use ---
static void test_equal_cost_no_dup(void) {
    float dx[3] = {0,    1000, 0};
    float dy[3] = {0,    0,    0};
    bool  dv[3] = {true, true, false};
    float tx[3] = {500,  500,  0}; // both tracks equidistant-ish
    float ty[3] = {0,    0,    0};
    bool  tv[3] = {true, true, false};
    uint8_t m[3];
    run(dx, dy, dv, tx, ty, tv, m);
    // Each detection used at most once.
    TEST_ASSERT_TRUE(m[0] != m[1] || m[0] == NO_MATCH);
    // Both tracks should be matched (2 det, 2 trk, all in gate).
    TEST_ASSERT_NOT_EQUAL(NO_MATCH, m[0]);
    TEST_ASSERT_NOT_EQUAL(NO_MATCH, m[1]);
}

// --- 8: 3 det, 3 trk full permutation optimal ---
static void test_3det_3trk_full(void) {
    float dx[3] = {0,    1000, 2000};
    float dy[3] = {0,    0,    0};
    bool  dv[3] = {true, true, true};
    // shuffled tracks
    float tx[3] = {2010, 10,   1010};
    float ty[3] = {0,    0,    0};
    bool  tv[3] = {true, true, true};
    uint8_t m[3];
    run(dx, dy, dv, tx, ty, tv, m);
    TEST_ASSERT_EQUAL_UINT8(2, m[0]); // trk0(2010) -> det2(2000)
    TEST_ASSERT_EQUAL_UINT8(0, m[1]); // trk1(10)   -> det0(0)
    TEST_ASSERT_EQUAL_UINT8(1, m[2]); // trk2(1010) -> det1(1000)
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_1det_1trk_match);
    RUN_TEST(test_2det_3trk_best_subset);
    RUN_TEST(test_3det_2trk_surplus_new);
    RUN_TEST(test_crossing);
    RUN_TEST(test_out_of_gate);
    RUN_TEST(test_no_det);
    RUN_TEST(test_equal_cost_no_dup);
    RUN_TEST(test_3det_3trk_full);
    return UNITY_END();
}
