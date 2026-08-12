#ifndef DSMR_BENCH_FIELD_PROFILES_H
#define DSMR_BENCH_FIELD_PROFILES_H

// Fixed-overhead reference, identical to the minimal library example.
#define DSMR_BENCH_MINIMAL_FIELDS \
  identification, \
  power_delivered

// Production profile copied from MyData in P1-Dongel-ESP32/DSMRloggerAPI.h.
// Commented-out fields in that source are intentionally not included.
#define DSMR_BENCH_P1_DONGLE_FIELDS \
  identification, \
  p1_version, \
  p1_version_be, \
  peak_pwr_last_q, \
  highest_peak_pwr, \
  timestamp, \
  equipment_id, \
  energy_delivered_tariff1, \
  energy_delivered_tariff2, \
  energy_returned_tariff1, \
  energy_returned_tariff2, \
  energy_delivered_total, \
  energy_returned_total, \
  electricity_tariff, \
  power_delivered, \
  power_returned, \
  voltage_l1, \
  voltage_l2, \
  voltage_l3, \
  current_l1, \
  current_l2, \
  current_l3, \
  power_delivered_l1, \
  power_delivered_l2, \
  power_delivered_l3, \
  power_returned_l1, \
  power_returned_l2, \
  power_returned_l3, \
  mbus1_device_type, \
  mbus1_equipment_id_tc, \
  mbus1_equipment_id_ntc, \
  mbus1_delivered, \
  mbus1_delivered_ntc, \
  mbus1_delivered_dbl, \
  mbus2_device_type, \
  mbus2_equipment_id_tc, \
  mbus2_equipment_id_ntc, \
  mbus2_delivered, \
  mbus2_delivered_ntc, \
  mbus2_delivered_dbl, \
  mbus3_device_type, \
  mbus3_equipment_id_tc, \
  mbus3_equipment_id_ntc, \
  mbus3_delivered, \
  mbus3_delivered_ntc, \
  mbus3_delivered_dbl, \
  mbus4_device_type, \
  mbus4_equipment_id_tc, \
  mbus4_equipment_id_ntc, \
  mbus4_delivered, \
  mbus4_delivered_ntc, \
  mbus4_delivered_dbl

#if DSMR_BENCH_PROFILE == 1
  #define DSMR_BENCH_PROFILE_NAME "minimal"
  #define DSMR_BENCH_FIELDS DSMR_BENCH_MINIMAL_FIELDS
#elif DSMR_BENCH_PROFILE == 2
  #define DSMR_BENCH_PROFILE_NAME "p1-dongle"
  #define DSMR_BENCH_FIELDS DSMR_BENCH_P1_DONGLE_FIELDS
#else
  #error "DSMR_BENCH_PROFILE must be 1 (minimal) or 2 (p1-dongle)"
#endif

#endif
