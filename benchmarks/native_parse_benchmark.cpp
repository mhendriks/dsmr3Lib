#ifndef DSMR_BENCH_PUBLIC_HEADER
  #define DSMR_BENCH_PUBLIC_HEADER "dsmr3.h"
#endif

#ifndef DSMR_BENCH_VERSION_NAME
  #define DSMR_BENCH_VERSION_NAME "v3-template"
#endif

#include DSMR_BENCH_PUBLIC_HEADER

#include <chrono>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "field_profiles.h"
#include "../tests/fixtures/p1_generator_telegrams.h"

using BenchmarkData = ParsedData<DSMR_BENCH_FIELDS>;

static volatile uint64_t sink = 0;

enum class BenchmarkPath {
  Core,
  EndToEnd
};

static uint32_t runCorpus(
    size_t iterations,
    BenchmarkPath path,
    uint64_t& fieldErrors) {
  using namespace p1_generator_fixtures;
  uint32_t telegramErrors = 0;

  for (size_t iteration = 0; iteration < iterations; ++iteration) {
    for (size_t fixture = 0; fixture < count; ++fixture) {
      BenchmarkData data;
      const char *telegram = all[fixture].telegram;
      ParseResult<void> result;
      if (path == BenchmarkPath::Core) {
        const char *dataStart = telegram + 1;
        const char *dataEnd = strchr(dataStart, '!');
        result = dataEnd
            ? P1Parser::parse_data(&data, dataStart, dataEnd, false)
            : ParseResult<void>().fail(F("No telegram end marker found"), dataStart);
      } else {
        result = P1Parser::parse(
            &data, telegram, strlen(telegram), false, false);
      }

      telegramErrors += result.err != NULL;
#if DSMR_BENCH_VERSION == 3
      fieldErrors += result.field_errors;
#endif
      if (data.power_delivered_present)
        sink += data.power_delivered.int_val();
    }
  }

  return telegramErrors;
}

static int benchmark(size_t iterations, BenchmarkPath path, const char *pathName) {
  uint64_t warmupFieldErrors = 0;
  runCorpus(10, path, warmupFieldErrors);

  uint64_t fieldErrors = 0;
  const std::chrono::steady_clock::time_point start =
      std::chrono::steady_clock::now();
  const uint32_t telegramErrors = runCorpus(iterations, path, fieldErrors);
  const std::chrono::steady_clock::time_point finish =
      std::chrono::steady_clock::now();

  const uint64_t telegrams = iterations * p1_generator_fixtures::count;
  const uint64_t elapsedNanoseconds =
      std::chrono::duration_cast<std::chrono::nanoseconds>(finish - start).count();
  const double nanosecondsPerTelegram =
      static_cast<double>(elapsedNanoseconds) / telegrams;

  printf(
      "version=%s profile=%s path=%s fields_object_bytes=%lu telegrams=%llu "
      "total_ms=%.3f ns_per_telegram=%.1f telegram_errors=%lu "
      "field_errors=%llu sink=%llu\n",
      DSMR_BENCH_VERSION_NAME,
      DSMR_BENCH_PROFILE_NAME,
      pathName,
      static_cast<unsigned long>(sizeof(BenchmarkData)),
      static_cast<unsigned long long>(telegrams),
      elapsedNanoseconds / 1000000.0,
      nanosecondsPerTelegram,
      static_cast<unsigned long>(telegramErrors),
      static_cast<unsigned long long>(fieldErrors),
      static_cast<unsigned long long>(sink));

  return telegramErrors == 0 ? 0 : 1;
}

int main(int argc, char **argv) {
  const size_t iterations = argc > 1
      ? static_cast<size_t>(strtoul(argv[1], NULL, 10))
      : 1000;

  const int coreResult = benchmark(iterations, BenchmarkPath::Core, "core");
  const int endToEndResult = benchmark(
      iterations, BenchmarkPath::EndToEnd, "end-to-end");
  return coreResult || endToEndResult ? 1 : 0;
}
