#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
project_dir=$(dirname -- "$script_dir")
dsmr2_dir=${DSMR2LIB_PATH:-"$(dirname -- "$project_dir")/dsmr2Lib"}
fqbn=${FQBN:-esp32:esp32:esp32c3:FlashSize=4M,PartitionScheme=min_spiffs}
iterations=${ITERATIONS:-1000}
work_dir=$(mktemp -d "${TMPDIR:-/tmp}/dsmr-benchmark.XXXXXX")
standard_dir=${DSMR3_STANDARD_PATH:-"$work_dir/dsmr3-standard"}

if [ ! -f "$dsmr2_dir/src/dsmr2.h" ]; then
  echo "dsmr2Lib not found at $dsmr2_dir" >&2
  exit 1
fi

if [ ! -f "$standard_dir/src/dsmr3.h" ]; then
  mkdir -p "$standard_dir"
  git -C "$project_dir" archive main | tar -x -C "$standard_dir"
fi

cmake -S "$project_dir" -B "$work_dir/native" \
  -DDSMR2LIB_PATH="$dsmr2_dir" \
  -DDSMR3_STANDARD_PATH="$standard_dir" >/dev/null
cmake --build "$work_dir/native" \
  --target dsmr_benchmarks dsmr3_standard_benchmarks -j4 >/dev/null

echo "Native parse benchmark ($iterations corpus iterations)"
for profile in minimal p1_dongle; do
  "$work_dir/native/dsmr2_benchmark_$profile" "$iterations"
  "$work_dir/native/dsmr3_standard_benchmark_$profile" "$iterations"
  "$work_dir/native/dsmr3_benchmark_$profile" "$iterations"
done

echo
echo "Arduino size benchmark ($fqbn)"
printf '%-9s %-8s %12s %12s\n' version profile flash_bytes static_ram

for variant in v2 v3-template v3-hybrid; do
  case "$variant" in
    v2) version=2; library_dir=$dsmr2_dir ;;
    v3-template) version=3; library_dir=$standard_dir ;;
    v3-hybrid) version=4; library_dir=$project_dir ;;
  esac
  for profile_number in 1 2; do
    case "$profile_number" in
      1) profile=minimal ;;
      2) profile=p1-dongle ;;
    esac

    build_dir="$work_dir/arduino-${variant}-${profile}"
    result_file="$work_dir/arduino-${variant}-${profile}.json"
    arduino-cli compile --json \
      --fqbn "$fqbn" \
      --build-path "$build_dir" \
      --library "$library_dir" \
      --build-property "compiler.cpp.extra_flags=-DDSMR_BENCH_VERSION=$version -DDSMR_BENCH_PROFILE=$profile_number" \
      "$script_dir/arduino_size" > "$result_file"

    flash=$(jq -r '.builder_result.executable_sections_size[] | select(.name == "text") | .size' "$result_file")
    ram=$(jq -r '.builder_result.executable_sections_size[] | select(.name == "data") | .size' "$result_file")
    printf '%-9s %-8s %12s %12s\n' "$variant" "$profile" "$flash" "$ram"
  done
done

echo
echo "Temporary build data: $work_dir"
