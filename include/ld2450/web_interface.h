#pragma once
// Web dashboard (single-page bilingual CS/EN GUI), served from PROGMEM as index_html.
//
// The page is split into four raw-string fragments so each concern can be edited in
// isolation without loading the whole ~1300-line document:
//   web/parts/head_css.inc — <head> + <style> CSS
//   web/parts/body.inc      — HTML body / layout
//   web/parts/i18n.inc      — <script> open + the CS/EN translation dictionary
//   web/parts/app_js.inc    — dashboard JavaScript + </script></body></html>
//
// Each .inc holds one raw string literal; #include pastes them as adjacent string
// literals which the compiler concatenates into a single PROGMEM blob — byte-for-byte
// identical to the previous inline page. (Multi-line raw strings can't live inside a
// #define, so we concatenate via #include instead.)
const char index_html[] PROGMEM =
#include "ld2450/web/parts/head_css.inc"
#include "ld2450/web/parts/body.inc"
#include "ld2450/web/parts/i18n.inc"
#include "ld2450/web/parts/app_js.inc"
;
