#pragma once
// SEC-03 (lean) — pure, host-testable web-credential predicates.
//
// Product decision: this is a public GitHub project. The shipped default is
// admin/admin and the operator changes it after install — we only *warn*, we do not
// block. These helpers keep that policy in one testable place.

#include <string.h>

namespace ld2450 {

// True while the web credentials are still the shipped default (defUser/defPass) or
// empty. Surfaced as a warning banner + Serial notice; never used to block requests.
inline bool isDefaultCreds(const char* user, const char* pass,
                           const char* defUser, const char* defPass) {
    if (!user || !pass) return true;
    if (user[0] == '\0' || pass[0] == '\0') return true;
    if (!defUser || !defPass) return false;
    return strcmp(user, defUser) == 0 && strcmp(pass, defPass) == 0;
}

// Whether a newly submitted web password is acceptable. Intentionally light for this
// open-source project (owner decision): only an empty password is rejected.
inline bool isAcceptableNewPassword(const char* pass) {
    return pass != nullptr && pass[0] != '\0';
}

}  // namespace ld2450
