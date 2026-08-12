#ifndef DSMR_BENCH_VERSION
  #define DSMR_BENCH_VERSION 3
#endif

#ifndef DSMR_BENCH_PROFILE
  #define DSMR_BENCH_PROFILE 1
#endif

#if DSMR_BENCH_VERSION == 2
  #include <dsmr2.h>
#elif DSMR_BENCH_VERSION == 3
  #include <dsmr3.h>
#else
  #error "DSMR_BENCH_VERSION must be 2 or 3"
#endif

#include "../field_profiles.h"

using BenchmarkData = ParsedData<DSMR_BENCH_FIELDS>;

static const char telegram[] =
    "/BENCHMARK\r\n"
    "1-3:0.2.8(50)\r\n"
    "0-0:1.0.0(260812120000S)\r\n"
    "0-0:96.1.1(45303030363030303030303030303030)\r\n"
    "1-0:1.8.0(001234.567*kWh)\r\n"
    "1-0:2.8.0(000123.456*kWh)\r\n"
    "1-0:1.8.1(000600.000*kWh)\r\n"
    "1-0:1.8.2(000634.567*kWh)\r\n"
    "1-0:2.8.1(000100.000*kWh)\r\n"
    "1-0:2.8.2(000023.456*kWh)\r\n"
    "0-0:96.14.0(0001)\r\n"
    "1-0:1.7.0(01.234*kW)\r\n"
    "1-0:2.7.0(00.123*kW)\r\n"
    "1-0:32.7.0(230.1*V)\r\n"
    "1-0:52.7.0(231.2*V)\r\n"
    "1-0:72.7.0(229.8*V)\r\n"
    "1-0:31.7.0(005*A)\r\n"
    "1-0:51.7.0(004*A)\r\n"
    "1-0:71.7.0(006*A)\r\n"
    "1-0:21.7.0(00.400*kW)\r\n"
    "1-0:41.7.0(00.410*kW)\r\n"
    "1-0:61.7.0(00.424*kW)\r\n"
    "1-0:22.7.0(00.040*kW)\r\n"
    "1-0:42.7.0(00.041*kW)\r\n"
    "1-0:62.7.0(00.042*kW)\r\n"
    "0-1:24.1.0(003)\r\n"
    "0-1:24.2.1(260812120000S)(00123.456*m3)\r\n"
    "!";

BenchmarkData benchmarkData;
volatile uint32_t benchmarkSink;

void setup() {
  ParseResult<void> result = P1Parser::parse(
      &benchmarkData, telegram, sizeof(telegram) - 1, false, false);
  benchmarkSink = result.err ? 1 : 0;
  if (benchmarkData.power_delivered_present)
    benchmarkSink += benchmarkData.power_delivered.int_val();
}

void loop() {
}

