# dsmr3Lib

Lightweight Arduino reader and parser for DSMR/P1 telegrams. `dsmr3Lib` is
intended as the source-compatible successor to `dsmr2Lib`, with tolerant
field-level parsing and automatic CRC handling.

## Version 3 contract

- A telegram does not need to contain a CRC.
- When four CRC digits are present after `!`, the CRC must be valid. A malformed
  or mismatching CRC rejects the complete telegram.
- An invalid requested field is skipped. Other valid requested fields in the
  same telegram remain available.
- Fields that were not requested through `ParsedData<...>` are ignored, just as
  in `dsmr2Lib`.
- A skipped field has its existing `field_present` member set to `false`. Zero
  remains a valid value and is never used as an error sentinel.
- No parser modes are introduced.
- The public classes, field names and result values stay compatible with
  `dsmr2Lib`. Migration should require only the dependency and main include to
  change from `dsmr2.h` to `dsmr3.h`.
- Internal headers and include guards use v3-specific names, so both Arduino
  libraries can be installed side by side.
- The implementation avoids exceptions, RTTI, dynamic error collections and
  other unnecessary embedded overhead.

See [DESIGN.md](DESIGN.md) for the normative decisions and acceptance criteria.
Architecture experiments are measured with the reproducible suite described in
[BENCHMARKS.md](BENCHMARKS.md).

## Tests

The native test corpus is generated into
`tests/fixtures/p1_generator_telegrams.h` from all static telegrams in the
`p1_generator` sketch:

```sh
./tools/sync_p1_generator_fixtures.sh ../p1_generator
cmake -S . -B build -DDSMR2LIB_PATH=../dsmr2Lib
cmake --build build
ctest --test-dir build --output-on-failure
```

The same compatibility source is compiled once against v2 and once against v3.
Its canonical output is compared byte-for-byte. Dedicated v3 tests cover
optional, valid, invalid and malformed CRCs plus invalid-field recovery.

The checked-in fixture header makes the tests reproducible. Re-run the sync
script whenever telegrams are added to `p1_generator` and commit the result.
