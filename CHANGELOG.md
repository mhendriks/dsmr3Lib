# Changelog

All notable changes to `dsmr3Lib` are documented in this file.

## [1.0.0] - 2026-08-16

First stable public release.

### Highlights

- Retains the public classes, field names, value types and `ParsedData<...>`
  interface from `dsmr2Lib`; most sketches only need to replace
  `#include <dsmr2.h>` with `#include <dsmr3.h>`.
- Isolates malformed requested fields. The invalid field remains absent while
  later valid fields in the same telegram are still parsed.
- Accepts telegrams with or without a CRC and always validates a CRC when one
  is present.
- Detects CRC presence from the first three valid observations using a
  two-out-of-three majority, then avoids CRC work for meters without a CRC.
- Adds cumulative reader diagnostics for CRC mode, CRC errors and skipped
  requested fields.
- Adds optional `P1FieldWarning` output containing the skipped-field count and
  the first skipped input line.
- Accepts a final data field immediately before `!`, without requiring CR/LF.
- Adds `CompleteRaw(String&)` so firmware can reuse a destination buffer and
  detect allocation failure; the original `CompleteRaw()` API remains.
- Shares value decoders between fields. In the measured P1 Dongle profile this
  saves 1,082 bytes of ESP32-C3 flash compared with dsmr2Lib, with unchanged
  static RAM usage.
- Keeps v2 and v3 installable side by side through version-specific headers and
  include guards.

### Validation

- Compatibility snapshots compare all generated corpus telegrams against
  `dsmr2Lib` byte for byte.
- Profile snapshots compare presence flags and values for the complete
  production P1 Dongle field selection.
- Resilience tests cover optional, valid, malformed and mismatching CRCs,
  invalid-field recovery, reader diagnostics and non-CR/LF-terminated final
  fields.
- All eight native and compatibility tests pass for this release.

### Integration note

DSMR-API firmware 5.9.0 uses the v3-only `P1FieldWarning`, `P1Diagnostics` and
`CompleteRaw(String&)` APIs and must be built with dsmr3Lib 1.0.0 or a newer
compatible release.

[1.0.0]: https://github.com/mhendriks/dsmr3Lib/releases/tag/v1.0.0
