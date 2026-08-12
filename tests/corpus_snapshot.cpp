#ifndef DSMR_TEST_PUBLIC_HEADER
#define DSMR_TEST_PUBLIC_HEADER "dsmr3.h"
#endif

#include DSMR_TEST_PUBLIC_HEADER

#include <stdio.h>
#include <string.h>

#include "fixtures/p1_generator_telegrams.h"

using SnapshotData = ParsedData<
    identification,
    p1_version,
    timestamp,
    equipment_id,
    energy_delivered_total,
    energy_returned_total,
    energy_delivered_tariff1,
    energy_delivered_tariff2,
    energy_returned_tariff1,
    energy_returned_tariff2,
    electricity_tariff,
    power_delivered,
    power_returned,
    voltage_l1,
    voltage_l2,
    voltage_l3,
    current_l1,
    current_l2,
    current_l3,
    power_delivered_l1,
    power_delivered_l2,
    power_delivered_l3,
    power_returned_l1,
    power_returned_l2,
    power_returned_l3,
    mbus1_device_type,
    mbus1_delivered>;

static void printString(bool present, const String& value) {
  if (present) printf("1:%s", value.c_str());
  else printf("0:");
}

static void printFixed(bool present, FixedValue& value) {
  if (present) printf("1:%lu", static_cast<unsigned long>(value.int_val()));
  else printf("0:");
}

static void printTimestamped(bool present, TimestampedFixedValue& value) {
  if (present) {
    printf("1:%lu@%s", static_cast<unsigned long>(value.int_val()),
           value.timestamp.c_str());
  } else {
    printf("0:");
  }
}

int main() {
  using namespace p1_generator_fixtures;
  printf("fixtures=%lu\n", static_cast<unsigned long>(count));

  for (size_t i = 0; i < count; ++i) {
    SnapshotData data;
    const char *telegram = all[i].telegram;
    ParseResult<void> result = P1Parser::parse(
        &data, telegram, strlen(telegram), false, false);

    printf("case|%s|%u|", all[i].name, result.err ? 0U : 1U);
    printString(data.identification_present, data.identification); printf("|");
    printString(data.p1_version_present, data.p1_version); printf("|");
    printString(data.timestamp_present, data.timestamp); printf("|");
    printString(data.equipment_id_present, data.equipment_id); printf("|");
    printFixed(data.energy_delivered_total_present, data.energy_delivered_total); printf("|");
    printFixed(data.energy_returned_total_present, data.energy_returned_total); printf("|");
    printFixed(data.energy_delivered_tariff1_present, data.energy_delivered_tariff1); printf("|");
    printFixed(data.energy_delivered_tariff2_present, data.energy_delivered_tariff2); printf("|");
    printFixed(data.energy_returned_tariff1_present, data.energy_returned_tariff1); printf("|");
    printFixed(data.energy_returned_tariff2_present, data.energy_returned_tariff2); printf("|");
    printString(data.electricity_tariff_present, data.electricity_tariff); printf("|");
    printFixed(data.power_delivered_present, data.power_delivered); printf("|");
    printFixed(data.power_returned_present, data.power_returned); printf("|");
    printFixed(data.voltage_l1_present, data.voltage_l1); printf("|");
    printFixed(data.voltage_l2_present, data.voltage_l2); printf("|");
    printFixed(data.voltage_l3_present, data.voltage_l3); printf("|");
    printFixed(data.current_l1_present, data.current_l1); printf("|");
    printFixed(data.current_l2_present, data.current_l2); printf("|");
    printFixed(data.current_l3_present, data.current_l3); printf("|");
    printFixed(data.power_delivered_l1_present, data.power_delivered_l1); printf("|");
    printFixed(data.power_delivered_l2_present, data.power_delivered_l2); printf("|");
    printFixed(data.power_delivered_l3_present, data.power_delivered_l3); printf("|");
    printFixed(data.power_returned_l1_present, data.power_returned_l1); printf("|");
    printFixed(data.power_returned_l2_present, data.power_returned_l2); printf("|");
    printFixed(data.power_returned_l3_present, data.power_returned_l3); printf("|");
    if (data.mbus1_device_type_present)
      printf("1:%u|", static_cast<unsigned>(data.mbus1_device_type));
    else
      printf("0:|");
    printTimestamped(data.mbus1_delivered_present, data.mbus1_delivered);
    printf("\n");
  }
}

