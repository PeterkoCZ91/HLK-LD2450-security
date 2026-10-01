#pragma once
// "ssid,password" split on the LAST comma (so an SSID may contain commas).
#include <string>

namespace ld2450_ble {

inline bool splitWifiCred(const std::string& v, std::string& ssid, std::string& pass) {
    size_t c = v.rfind(',');
    if (c == std::string::npos || c == 0) return false;
    ssid = v.substr(0, c);
    pass = v.substr(c + 1);
    return true;
}

}  // namespace ld2450_ble
