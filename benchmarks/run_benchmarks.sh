#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
project_dir=$(dirname -- "$script_dir")
dsmr2_dir=${DSMR2LIB_PATH:-"$(dirname -- "$project_dir")/dsmr2Lib"}
fqbn=${FQBN:-esp32:esp32:esp32c3:FlashSize=4M,PartitionScheme=min_spiffs}
iterations=${ITERATIONS:-1000}
work_dir=$(mktemp -d "${TMPDIR:-/tmp}/dsmr-benchmark.XXXXXX")

if [ ! -f "$dsmr2_dir/src/dsmr2.h" ]; then
  echo "dsmr2Lib not found at $dsmr2_dir" >&2
  exit 1
fi

cmake -S "$project_dir" -B "$work_dir/native" -DDSMR2LIB_PATH="$dsmr2_dir" >/dev/null
cmake --build "$work_dir/native" --target dsmr_benchmarks -j4 >/dev/null

echo "Native parse benchmark ($iterations corpus iterations)"
for profile in minimal p1_dongle; do
  "$work_dir/native/dsmr2_benchmark_$profile" "$iterations"
  "$work_dir/native/dsmr3_benchmark_$profile" "$iterations"
done

echo
echo "Arduino size benchmark ($fqbn)"
printf '%-9s %-8s %12s %12s\n' version profile flash_bytes static_ram

for version in 2 3; do
  for profile_number in 1 2; do
    case "$profile_number" in
      1) profile=minimal ;;
      2) profile=p1-dongle ;;
    esac

    build_dir="$work_dir/arduino-v${version}-${profile}"
    result_file="$work_dir/arduino-v${version}-${profile}.json"
    arduino-cli compile --json \
      --fqbn "$fqbn" \
      --build-path "$build_dir" \
      --library "$dsmr2_dir" \
      --library "$project_dir" \
      --build-property "compiler.cpp.extra_flags=-DDSMR_BENCH_VERSION=$version -DDSMR_BENCH_PROFILE=$profile_number" \
      "$script_dir/arduino_size" > "$result_file"

    flash=$(jq -r '.builder_result.executable_sections_size[] | select(.name == "text") | .size' "$result_file")
    ram=$(jq -r '.builder_result.executable_sections_size[] | select(.name == "data") | .size' "$result_file")
    printf 'v%-8s %-8s %12s %12s\n' "$version" "$profile" "$flash" "$ram"
  done
done

echo
echo "Temporary build data: $work_dir"
