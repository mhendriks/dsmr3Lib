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

By default the script extracts `main` to a temporary directory as the standard
v3 reference. Set `DSMR3_STANDARD_PATH` to compare with a different checkout.

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

## Hybrid dispatch experiment

Branch `codex/hybrid-parser` replaces the recursive field lookup with a small
runtime core. Templates still define storage and generate one typed callback
per selected field, while the shared parser searches a compact OBIS-ID array
and invokes the matching callback. The public `ParsedData<...>` API, field
types and parser behavior remain unchanged.

Measured on 2026-08-13 with the same ESP32 core and FQBN as the baseline:

| Version | Profile | Flash | Static RAM | Host `ParsedData` size |
| --- | --- | ---: | ---: | ---: |
| v3 template | minimal | 265,150 B | 13,208 B | 40 B |
| v3 hybrid | minimal | 265,396 B | 13,208 B | 40 B |
| v3 template | P1 Dongle | 287,800 B | 13,960 B | 1,168 B |
| v3 hybrid | P1 Dongle | 290,082 B | 13,960 B | 1,168 B |

The hybrid variant is 246 bytes larger for the minimal profile and 2,282 bytes
larger for the P1 Dongle profile. Static RAM and `ParsedData` size are exactly
equal. A 1,000-iteration native run found both field-dispatch variants in the
same performance range; the result varies by a few percent between runs and
does not justify the flash increase. All corpus snapshots match v2 and the
standard v3 byte-for-byte.

Conclusion: this particular function-pointer hybrid does not meet the
acceptance criteria and should not replace the standard v3 implementation.
The branch is retained as a measured prototype. A follow-up architecture would
need to share the value decoders themselves (for example compact descriptors
plus offsets), not merely replace template recursion with callbacks, before it
is likely to reduce flash.
