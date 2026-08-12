# dsmr3Lib design contract

This document is normative for version 3.

## Telegram integrity

| Input after `!` | Result |
| --- | --- |
| nothing or line ending | accept without CRC |
| exactly four hexadecimal digits | validate and accept only on a match |
| one to three digits or non-hex data | reject as malformed CRC |
| validly formatted but mismatching CRC | reject the complete telegram |

CRC failure is a telegram-level failure because corruption can affect every
field, including fields that appear syntactically valid.

## Field isolation

Each requested field is parsed transactionally:

1. Match the OBIS identifier.
2. Parse the complete field into its existing value type.
3. Set `field_present = true` only after successful parsing of the complete
   line.
4. On a field error, leave `field_present = false`, record lightweight
   diagnostics in `ParseResult`, advance to the next line and continue.

Malformed OBIS lines are skipped as invalid fields. Empty lines and unrequested
OBIS fields are ignored. Framing errors, such as a missing `/` or `!`, remain
telegram errors.

## Compatibility definition

For every telegram accepted by `dsmr2Lib`, parsing the same `ParsedData` field
set with v3 must produce:

- the same overall success result;
- the same `*_present` flags;
- the same integer/string values and timestamps.

The compatibility suite compiles the same source separately against v2 and v3
because both libraries intentionally retain the `dsmr` namespace and public
type names. The resulting canonical snapshots are compared byte-for-byte.

V3-only resilience tests cover telegrams rejected by v2. They assert that an
invalid field is absent while later valid fields are still available.

## Embedded constraints

- C++11 and Arduino 1.6.6 or newer.
- No exceptions or RTTI required.
- No heap-backed collection of errors.
- Parsing remains selective through `ParsedData<...>` templates.
- Diagnostics add only counters and pointers to static messages/input data.
- Reader buffers retain their existing v2 behavior and fixed-buffer option.

## Coexistence

The public entry point is `dsmr3.h`; internal headers live under `dsmr3/` and
use `DSMR3_` include guards. The public namespace remains `dsmr` for source
compatibility. V2 and v3 may both be installed, but should not be included in
the same translation unit because their public types deliberately have the same
names.

