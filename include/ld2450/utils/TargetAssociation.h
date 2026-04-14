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
 * For 3 targets, brute-force over all 6 permutations is faster than
 * full Hungarian Algorithm and uses zero dynamic allocation.
 */

static constexpr float ASSOCIATION_MAX_DIST = 2000.0f; // mm
static constexpr uint8_t NO_MATCH = 0xFF;

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

    // --- Match detections to active tracks ---
    if (nTrk > 0) {
        // Build cost matrix
        float cost[3][3];
        for (int t = 0; t < nTrk; t++) {
            for (int d = 0; d < nDet; d++) {
                float dx = detX[detIdx[d]] - trkX[trkIdx[t]];
                float dy = detY[detIdx[d]] - trkY[trkIdx[t]];
                cost[t][d] = sqrtf(dx*dx + dy*dy);
            }
        }

        // Brute-force optimal assignment (max 3! = 6 permutations)
        uint8_t bestAssign[3] = {NO_MATCH, NO_MATCH, NO_MATCH};
        float bestCost = 1e9f;
        int maxPerm = (nDet <= nTrk) ? nDet : nTrk;

        for (int a = 0; a < nDet; a++) {
            for (int b = 0; b < nDet; b++) {
                if (maxPerm > 1 && b == a) continue;
                for (int c = 0; c < nDet; c++) {
                    if (maxPerm > 2 && (c == a || c == b)) continue;

                    uint8_t perm[3] = {(uint8_t)a, (uint8_t)b, (uint8_t)c};
                    float totalCost = 0;
                    bool valid = true;
                    for (int t = 0; t < nTrk && t < maxPerm; t++) {
                        float cv = cost[t][perm[t]];
                        if (cv > ASSOCIATION_MAX_DIST) { valid = false; break; }
                        totalCost += cv;
                    }

                    if (valid && totalCost < bestCost) {
                        bestCost = totalCost;
                        for (int t = 0; t < nTrk && t < maxPerm; t++)
                            bestAssign[t] = perm[t];
                    }

                    if (maxPerm <= 2) break;
                }
                if (maxPerm <= 1) break;
            }
        }

        // Apply best assignment
        for (int t = 0; t < nTrk; t++) {
            if (bestAssign[t] != NO_MATCH)
                mapping[trkIdx[t]] = detIdx[bestAssign[t]];
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
