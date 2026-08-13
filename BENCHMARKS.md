# Parser architecture benchmarks

This benchmark suite exists to decide whether the hybrid parser core is an
actual improvement over the current template implementation. A rewrite is not
accepted on code structure alone.

## Measurements

Two identical field profiles are compiled against dsmr2Lib and dsmr3Lib:

| Profile | Purpose |
| --- | --- |
| `minimal` | Fixed overhead reference: identification and power |
| `p1-dongle` | Exact active `MyData` fields from `P1-Dongel-ESP32/DSMRloggerAPI.h` at commit `12ae500` |

The P1 Dongle profile is the primary decision profile. The minimal profile is
retained only to reveal whether a shared parser core imposes disproportionate
fixed overhead on very small integrations.

The suite reports:

- native core parse time, excluding framing and CRC;
- native end-to-end time, including v3 automatic CRC detection;
- `sizeof(ParsedData<...>)` for each profile;
- ESP32-C3 program-storage usage;
- ESP32-C3 static/global RAM usage;
- telegram and field error counts.

Run it from the repository root:

```sh
./benchmarks/run_benchmarks.sh
```

Optional environment variables:

```sh
ITERATIONS=5000 FQBN='esp32:esp32:esp32c3:FlashSize=4M,PartitionScheme=min_spiffs' \
  ./benchmarks/run_benchmarks.sh
```

Native timings are useful for relative comparisons on the same machine and
build. They are not a substitute for a later on-device cycle benchmark.

## Acceptance criteria for the hybrid core

The hybrid implementation must:

1. Keep all compatibility and resilience tests green.
2. Produce zero telegram errors for the generator corpus.
3. Never regress static RAM for either profile.
4. Improve flash or speed for the P1 Dongle profile without making the minimal
   profile materially worse.
5. Perform no heap allocation in framing, CRC validation or line tokenization.
6. Retain the existing public `ParsedData<...>` API.

A reasonable initial threshold for “materially worse” is more than 5% flash or
10% parse-time regression. These thresholds can be tightened after collecting
the baseline on the target boards.

## Template baseline

Measured on 2026-08-12 with Arduino ESP32 core 3.3.11 and:

```text
esp32:esp32:esp32c3:FlashSize=4M,PartitionScheme=min_spiffs
```

| Version | Profile | Flash | Static RAM | Host `ParsedData` size |
| --- | --- | ---: | ---: | ---: |
| v2 template | minimal | 264,026 B | 13,208 B | 40 B |
| v3 template | minimal | 265,150 B | 13,208 B | 40 B |
| v2 template | P1 Dongle | 279,756 B | 13,960 B | 1,168 B |
| v3 template | P1 Dongle | 287,800 B | 13,960 B | 1,168 B |

The host object size uses the native Arduino `String` test stub and must not be
interpreted as ESP32 object size. The ESP32 static-RAM comparison is the
authoritative embedded measurement.

Important observations:

- V3 static RAM is equal to v2 for both profiles.
- V3 adds 1,124 bytes of flash for the minimal profile and 8,044 bytes for the
  production P1 Dongle profile.
- The isolated field parser is in the same performance range as v2. Repeated
  host runs show normal benchmark noise around a small single-digit difference.
- V3 end-to-end parsing is currently substantially slower because it always
  scans and calculates the optional CRC. V2 with `checksum=false` does not do
  equivalent integrity work, so this is not evidence that template dispatch is
  slower. Framing/CRC and field dispatch must be benchmarked separately.

The first hybrid target is therefore reducing the 22,650-byte v3 flash increase
from minimal to the P1 Dongle profile, while preserving equal static RAM and
the current tolerant behavior.

## Shared value-decoder experiment

Branch `codex/shared-value-decoders` keeps the standard v3 `ParsedData<...>`
storage and recursive field selection. Only the parsing bodies for strings,
timestamps, fixed values, timestamped fixed values and raw values move from
field templates to ordinary functions in `fields3.cpp`. This lets every field
retain its existing type and API while sharing one decoder per value shape.

Measured on 2026-08-13 with the same ESP32 core and FQBN as the baseline:

| Version | Profile | Flash | Static RAM | Host `ParsedData` size |
| --- | --- | ---: | ---: | ---: |
| v2 template | minimal | 264,026 B | 13,208 B | 40 B |
| v3 template decoders | minimal | 265,150 B | 13,208 B | 40 B |
| v3 shared decoders | minimal | 265,376 B | 13,208 B | 40 B |
| v2 template | P1 Dongle | 279,756 B | 13,960 B | 1,168 B |
| v3 template decoders | P1 Dongle | 287,800 B | 13,960 B | 1,168 B |
| v3 shared decoders | P1 Dongle | 278,674 B | 13,960 B | 1,168 B |

For the production profile, shared decoders save 9,126 bytes versus standard
v3 and 1,082 bytes versus v2. They add 226 bytes to the intentionally tiny
minimal profile, but this remains below the 5% acceptance limit. Static RAM
and the data-object size are unchanged.

A same-run 1,000-iteration native benchmark measured P1 Dongle core parsing at
9,856 ns/telegram for standard v3 and 9,014 ns/telegram for shared decoders.
End-to-end measurements remained in the same range and are dominated by CRC
work. Timing varies between host runs, so the flash reduction—not this single
speed result—is the primary evidence.

All 12 tests pass. Two snapshot suites compare every generator telegram with
v2 and standard v3; the second suite hashes presence and values for all 52 P1
Dongle fields. Both shared-decoder comparisons matched all 24 telegrams.

### Descriptor/offset decision

Compact lookup descriptors and storage offsets were not added to this branch.
The earlier function-pointer dispatch experiment showed that a per-field table
can consume more flash than recursive selection, while decoder sharing alone
already exceeds the production-profile target. Offsets would also couple the
parser to C++ base-class layout unless storage is redesigned. The safer next
step is therefore target-hardware soak testing of this smaller change. A
descriptor lookup should only be reconsidered if profiling later shows field
selection—not decoding or CRC—to be the remaining bottleneck.
