#pragma once
// SEC-01 — fail-closed authorization decisions for the chunked Web OTA upload.
//
// The AsyncWebServer OTA route has two callbacks: a body/upload callback that
// receives the firmware in chunks (and can call Update.begin/write/end), and a
// completion callback that sends the HTTP response. Authentication MUST gate the
// upload callback — checking auth only in the completion callback is too late,
// because the image has already been written to flash by then.
//
// These helpers are pure (no Arduino/AsyncWebServer dependency) so the security
// invariant is host-testable in the `native` test environment.

namespace ld2450 {

// Decide whether a firmware write may BEGIN for the first chunk (index == 0).
// Fails closed: a write may only start when the request authenticated AND no
// other OTA session currently owns the Update library. Any unauthenticated or
// concurrent (busy) attempt returns false and must never touch Update.
inline bool otaMayBegin(bool authenticated, bool otaActive) {
    return authenticated && !otaActive;
}

// Decide whether a subsequent data/final chunk may write to the Update library.
// Only the request that successfully began the active session holds an ownership
// token; unauthenticated requests never receive one, so their chunks can never
// pass this gate.
inline bool otaMayWrite(bool ownsSession) {
    return ownsSession;
}

}  // namespace ld2450
