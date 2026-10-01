#pragma once
// SEC-06 — pure, host-testable CSRF decision logic for state-changing HTTP routes.
//
// Management is HTTP Basic Auth, so a browser re-sends cached credentials to any
// request it is tricked into making (classic CSRF). We defend by validating the
// Origin/Referer authority against our own Host on mutating methods:
//   - absent Origin AND Referer  -> allow (non-browser API client: curl, Home
//     Assistant — carries no ambient browser credentials, not a CSRF vector)
//   - present but mismatched      -> reject (fail closed: cross-origin browser page)
//
// Kept free of Arduino/AsyncWebServer types so it runs in the `native` test env.

#include <string.h>
#include <strings.h>  // strncasecmp

namespace ld2450 {

// Does the host[:port] authority of an Origin/Referer URL match `host`
// (case-insensitive)? URLs look like "http://host[:port]" or
// "http://host[:port]/path". Returns false if no scheme is present.
inline bool csrfHostMatches(const char* url, const char* host) {
    if (!url || !host) return false;
    const char* p = strstr(url, "://");
    if (!p) return false;
    p += 3;
    const char* slash = strchr(p, '/');
    size_t len = slash ? (size_t)(slash - p) : strlen(p);
    return strlen(host) == len && strncasecmp(p, host, len) == 0;
}

// Decide whether a request passes CSRF validation.
//   mutating : false for GET/HEAD/OPTIONS (always allowed)
//   host     : value of the Host header (may be nullptr)
//   origin   : value of the Origin header (nullptr or "" when absent)
//   referer  : value of the Referer header (nullptr or "" when absent)
inline bool csrfAllowed(bool mutating, const char* host,
                        const char* origin, const char* referer) {
    if (!mutating) return true;
    if (origin && origin[0])  return csrfHostMatches(origin, host ? host : "");
    if (referer && referer[0]) return csrfHostMatches(referer, host ? host : "");
    return true;
}

}  // namespace ld2450
