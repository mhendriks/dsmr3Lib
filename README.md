# dsmr3Lib

`dsmr3Lib` is a lightweight Arduino reader and parser for Dutch smart-meter
DSMR/P1 telegrams. Version 1.0.0 is the source-compatible successor to
[`dsmr2Lib`](https://github.com/mhendriks/dsmr2Lib), with more resilient field
parsing, automatic CRC handling and compact runtime diagnostics.

The library is intended for embedded applications that must keep processing
useful meter data when a single requested field is malformed. It is used by
the DSMR-API firmware from version 5.9.0 onward.

## What is different from dsmr2Lib?

| Behaviour | dsmr2Lib | dsmr3Lib 1.0.0 |
| --- | --- | --- |
| Main include | `#include <dsmr2.h>` | `#include <dsmr3.h>` |
| Invalid requested field | Parsing stops with an error | Field is skipped; later valid fields remain available |
| Optional CRC | Controlled by the caller | Detected automatically and always validated when present |
| CRC detection | No persistent detection | Locks after a two-out-of-three majority of valid telegrams |
| Runtime insight | Parser error text | CRC mode/error counters, skipped-field counter and optional first-field warning |
| Final field directly before `!` | Requires a CR/LF separator | Accepted and parsed normally |
| Complete telegram copy | Returns a new `String` | Also supports a reusable destination `String` |
| Generated parsing code | Decoder logic instantiated through field templates | Common value decoders are shared to reduce production firmware size |

The existing namespace, field names, value types and `ParsedData<...>` API are
retained. For normal sketches, migration starts with changing the dependency
and the main include from `dsmr2.h` to `dsmr3.h`.

Both libraries can be installed side by side because their headers and include
guards have version-specific names. Do not include both in the same translation
unit: the public types deliberately use the same `dsmr` namespace and names.

## Version 1.0.0 behaviour

- A telegram may omit its CRC.
- If four CRC digits are present after `!`, they must be valid. A malformed or
  mismatching CRC rejects the complete telegram.
- An invalid requested field is skipped and its existing `field_present`
  member remains `false`. Zero remains a valid value and is not an error
  sentinel.
- Other valid requested fields in the telegram remain available.
- Fields not requested through `ParsedData<...>` are ignored, as in
  `dsmr2Lib`.
- A final data field may be followed immediately by `!`, without CR/LF.
- No parser modes, exceptions, RTTI or dynamic error collections are added.

See [DESIGN.md](DESIGN.md) for the normative parser contract.
Release history is recorded in [CHANGELOG.md](CHANGELOG.md).

## Installation

Clone or download this repository into the Arduino libraries directory, then
restart the Arduino IDE if it is running:

```sh
git clone https://github.com/mhendriks/dsmr3Lib.git
```

Include the public header in the sketch:

```cpp
#include <dsmr3.h>
```

The library requires C++11 and Arduino 1.6.6 or newer. It does not require
another Arduino library.

## Minimal parsing example

Only list fields that the application uses. This keeps generated code and the
`ParsedData` object small.

```cpp
#include <dsmr3.h>

using MyData = ParsedData<identification, power_delivered>;

const char telegram[] =
  "/KFM5KAIFA-METER\r\n"
  "1-0:1.7.0(00.318*kW)\r\n"
  "!\r\n";

MyData data;
ParseResult<void> result =
    P1Parser::parse(&data, telegram, lengthof(telegram));

if (!result.err && data.power_delivered_present) {
  Serial.println(data.power_delivered.int_val()); // watts
}
```

The complete examples are in [`examples/`](examples).

## Reading telegrams

`P1Reader` retains the Arduino `String`-based reader API. For
memory-constrained or long-running firmware, use
`P1FixedReader<MaxTelegramLength>`. The fixed reader receives data in a
fixed-size buffer and exposes it through `raw()` and `rawLength()`.

```cpp
P1FixedReader<2500> reader(&Serial1, requestPin);

void loop() {
  reader.loop();
  if (!reader.available()) return;

  MyData data;
  String error;
  P1FieldWarning warning;

  if (reader.parse(&data, &error, false, &warning)) {
    // Use every field whose *_present member is true.
  } else {
    Serial.println(error); // Telegram-level/framing error.
  }

  if (warning.count) {
    Serial.print("Skipped fields: ");
    Serial.println(warning.count);
    Serial.println(warning.line); // First skipped input line.
  }
}
```

`P1FieldWarning` is optional. Passing it does not turn a skipped field into a
telegram-level error.

## CRC detection and diagnostics

`P1FixedReader` and `P1Reader` expose cumulative diagnostics:

```cpp
P1Diagnostics info = reader.diagnostics();

// info.crc_mode: P1CrcMode::DETECTING, PRESENT or ABSENT
// info.crc_errors: rejected CRC suffixes or mismatches
// info.skipped_fields: requested fields skipped while parsing
```

CRC presence is determined from the first three valid observations and locked
using a two-out-of-three majority. Invalid CRC telegrams increase
`crc_errors`, but do not vote. Once a meter is detected as not supplying a
CRC, the reader skips CRC calculation for subsequent telegrams.

For source compatibility, `doChecksum(bool)` still exists. In v3 the argument
does not force a mode; calling the method resets CRC detection and all
diagnostic counters. `ChangeStream()` does the same.

`CompleteRaw()` remains available and returns a `String`. Firmware that reuses
a buffer can avoid an extra temporary allocation with:

```cpp
String capturedTelegram;
if (!reader.CompleteRaw(capturedTelegram)) {
  // The destination could not reserve enough memory.
}
```

The reconstructed telegram only contains CRC digits when the received telegram
contained a CRC.

## Compatibility and tests

The native compatibility suite parses the same generated telegram corpus with
v2 and v3 and compares presence flags and values byte for byte. Dedicated v3
tests cover optional, valid, invalid and malformed CRCs, field isolation,
reader diagnostics and a final field without CR/LF before `!`.

```sh
cmake -S . -B build -DDSMR2LIB_PATH=../dsmr2Lib
cmake --build build
ctest --test-dir build --output-on-failure
```

The test fixtures are generated from the `p1_generator` sketch and checked in
for reproducibility. Refresh them after adding telegrams to that project:

```sh
./tools/sync_p1_generator_fixtures.sh ../p1_generator
```

Reproducible parser and ESP32-C3 size measurements are documented in
[BENCHMARKS.md](BENCHMARKS.md). In the measured P1 Dongle profile, the shared
decoders use 1,082 bytes less flash than v2 while keeping static RAM unchanged.

## Release integration

DSMR-API firmware 5.9.0 depends on the v3-only reader API, including
`P1FieldWarning`, `P1Diagnostics` and `CompleteRaw(String&)`. Build that
firmware with dsmr3Lib 1.0.0 or a newer compatible release; dsmr2Lib cannot be
substituted without reverting those integrations.
