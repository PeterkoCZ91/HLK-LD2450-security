#pragma once
#include <cmath>
#include <cstdint>

/**
 * TargetAssociation - Match new radar detections to existing tracks
 *
 * LD2450 reports up to 3 targets but their slot indices shuffle between
 * frames. This finds the optimal assignment (minimum total distance)
 * between new detections and existing track positions.
 *
 * With at most 3 tracks and 3 detections the whole assignment space is
 * tiny, so we enumerate every injective partial matching by recursion.
 * The objective is lexicographic: first maximise the number of gated
 * matches, then minimise the total distance among matched pairs. This is
 * important when the counts differ:
 *   - nDet < nTrk: the *best subset* of tracks keeps following detections,
 *     the others are dropped (previously the last tracks were always
 *     dropped regardless of distance).
 *   - nDet > nTrk: every track keeps its closest detection and the surplus
 *     detections fall through to the "new target" phase below.
 * Uses zero dynamic allocation.
 */

static constexpr float ASSOCIATION_MAX_DIST = 2000.0f; // mm
static constexpr uint8_t NO_MATCH = 0xFF;

namespace ld2450_assoc_detail {

// Recurse over compacted track indices [t..nTrk). `used` marks detections
// already consumed. Tracks may be left unmatched (skip) so that a smaller
// but cheaper subset can win when a match would exceed the gate.
inline void solve(int t, int nTrk, int nDet,
                  const float cost[3][3], bool used[3],
                  uint8_t assign[3], int depthMatches, float depthCost,
                  uint8_t best[3], int& bestMatches, float& bestCost)
{
    if (t == nTrk) {
        if (depthMatches > bestMatches ||
            (depthMatches == bestMatches && depthCost < bestCost)) {
            bestMatches = depthMatches;
            bestCost = depthCost;
            for (int i = 0; i < nTrk; i++) best[i] = assign[i];
        }
        return;
    }

    // Option A: leave track t unmatched.
    assign[t] = NO_MATCH;
    solve(t + 1, nTrk, nDet, cost, used, assign,
          depthMatches, depthCost, best, bestMatches, bestCost);

    // Option B: match track t to any free, in-gate detection.
    for (int d = 0; d < nDet; d++) {
        if (used[d]) continue;
        float cv = cost[t][d];
        if (cv > ASSOCIATION_MAX_DIST) continue;
        used[d] = true;
        assign[t] = (uint8_t)d;
        solve(t + 1, nTrk, nDet, cost, used, assign,
              depthMatches + 1, depthCost + cv, best, bestMatches, bestCost);
        used[d] = false;
    }
    assign[t] = NO_MATCH;
}

} // namespace ld2450_assoc_detail

inline void associateTargets(
    const float detX[3], const float detY[3], const bool detValid[3],
    const float trkX[3], const float trkY[3], const bool trkValid[3],
    uint8_t mapping[3])
{
    uint8_t nDet = 0, nTrk = 0;
    uint8_t detIdx[3], trkIdx[3];
    for (int i = 0; i < 3; i++) {
        if (detValid[i]) detIdx[nDet++] = i;
        if (trkValid[i]) trkIdx[nTrk++] = i;
        mapping[i] = NO_MATCH;
    }

    if (nDet == 0) return;

    // --- Match detections to active tracks (optimal partial matching) ---
    if (nTrk > 0) {
        float cost[3][3];
        for (int t = 0; t < nTrk; t++) {
            for (int d = 0; d < nDet; d++) {
                float dx = detX[detIdx[d]] - trkX[trkIdx[t]];
                float dy = detY[detIdx[d]] - trkY[trkIdx[t]];
                cost[t][d] = sqrtf(dx * dx + dy * dy);
            }
        }

        bool used[3] = {false, false, false};
        uint8_t assign[3] = {NO_MATCH, NO_MATCH, NO_MATCH};
        uint8_t best[3] = {NO_MATCH, NO_MATCH, NO_MATCH};
        int bestMatches = -1;
        float bestCost = 1e9f;
        ld2450_assoc_detail::solve(0, nTrk, nDet, cost, used, assign,
                                   0, 0.0f, best, bestMatches, bestCost);

        // best[t] is a compacted detection index for compacted track t.
        for (int t = 0; t < nTrk; t++) {
            if (best[t] != NO_MATCH)
                mapping[trkIdx[t]] = detIdx[best[t]];
        }
    }

    // --- Assign unmatched detections to empty track slots (new targets) ---
    bool detUsed[3] = {false, false, false};
    for (int i = 0; i < 3; i++) {
        if (mapping[i] != NO_MATCH) detUsed[mapping[i]] = true;
    }
    for (int d = 0; d < nDet; d++) {
        if (detUsed[detIdx[d]]) continue;
        for (int t = 0; t < 3; t++) {
            if (mapping[t] == NO_MATCH && !trkValid[t]) {
                mapping[t] = detIdx[d];
                break;
            }
        }
    }
}
