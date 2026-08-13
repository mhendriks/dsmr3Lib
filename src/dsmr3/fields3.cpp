/**
 * Arduino DSMR parser.
 *
 * This software is licensed under the MIT License.
 *
 * Copyright (c) 2015 Matthijs Kooijman <matthijs@stdin.nl>
 *
 *------------------------------------------------------------------------------
 * Changed by Willem Aandewiel
 * In the original library it is assumed that the Mbus GAS meter is 
 * always connected to MBUS_ID 1. But this is wrong. Mostly on
 * an initial installation the GAS meter is at MBUS_ID 1 but if an other
 * meter is installed it is connected to the first free MBUS_ID. 
 * So you cannot make any assumption about what mbus is connected to
 * which MBUS_ID. Therfore it is also not possible to check the units
 * on the basis of the MBUS_ID. It can be anything.
 * My assumption is that the device_type of a GAS meter is always "3"
 * and that of, f.i. a water meter is always "5".
 * I hope I'm right but have not been able to verify this with the
 * original documenation. 
 *------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Field parsing functions
 */

#include "fields3.h"


using namespace dsmr;
using namespace dsmr::fields;

namespace {

int8_t hexNibble(char value) {
  if (value >= '0' && value <= '9') return value - '0';
  if (value >= 'A' && value <= 'F') return value - 'A' + 10;
  if (value >= 'a' && value <= 'f') return value - 'a' + 10;
  return -1;
}

bool hexByte(const String& value, size_t offset, uint8_t& result) {
  if (offset + 1 >= value.length()) return false;
  const int8_t high = hexNibble(value[offset]);
  const int8_t low = hexNibble(value[offset + 1]);
  if (high < 0 || low < 0) return false;
  result = (uint8_t)((high << 4) | low);
  return true;
}

} // namespace

ParseResult<void> ValueDecoder::string(
    String& value, size_t minlen, size_t maxlen,
    const char *str, const char *end) {
  ParseResult<String> result =
      StringParser::parse_string(minlen, maxlen, str, end);
  if (!result.err)
    value = result.result;
  return result;
}

ParseResult<void> ValueDecoder::timestamp(
    String& value, const char *str, const char *end) {
  ParseResult<String> regular = StringParser::parse_string(12, 13, str, end);
  if (!regular.err) {
    value = regular.result;
    return regular;
  }

  // MCS301 uses the 12-byte DLMS/COSEM date-time octet string:
  // year(2), month, day, weekday, hour, minute, second, hundredths,
  // deviation(2), clock status. Convert it to DSMR YYMMDDhhmmssS/W.
  ParseResult<String> dlms = StringParser::parse_string(24, 24, str, end);
  if (dlms.err) return dlms;

  uint8_t bytes[12];
  for (size_t i = 0; i < sizeof(bytes); ++i) {
    if (!hexByte(dlms.result, i * 2, bytes[i]))
      return ParseResult<void>().fail(
          F("Invalid DLMS date-time"), str + 1 + i * 2);
  }

  const uint16_t year = ((uint16_t)bytes[0] << 8) | bytes[1];
  if (year < 2000 || year > 2099 ||
      bytes[2] < 1 || bytes[2] > 12 || bytes[3] < 1 || bytes[3] > 31 ||
      bytes[5] > 23 || bytes[6] > 59 || bytes[7] > 59)
    return ParseResult<void>().fail(F("Invalid DLMS date-time"), str + 1);

  char converted[14];
  snprintf(converted, sizeof(converted), "%02u%02u%02u%02u%02u%02u%c",
           (unsigned)(year % 100), (unsigned)bytes[2], (unsigned)bytes[3],
           (unsigned)bytes[5], (unsigned)bytes[6], (unsigned)bytes[7],
           (bytes[11] & 0x80) ? 'S' : 'W');
  value = converted;
  return ParseResult<void>().until(dlms.next);
}

ParseResult<void> ValueDecoder::fixed(
    uint32_t& value, const char *unit,
    const char *str, const char *end) {
  ParseResult<uint32_t> result = NumParser::parse(3, unit, str, end);
  if (!result.err)
    value = result.result;
  return result;
}

ParseResult<void> ValueDecoder::timestampedFixed(
    String& timestamp, uint32_t& value, const char *unit,
    const char *str, const char *end) {
  // Normal order: timestamp, then value.
  ParseResult<String> ts = StringParser::parse_string(13, 13, str, end);
  if (ts.err)
    ts = StringParser::parse_string(12, 12, str, end);
  if (!ts.err) {
    timestamp = ts.result;
    return fixed(value, unit, ts.next, end);
  }

  // Walloon variant: value, then timestamp.
  ParseResult<void> parsedValue = fixed(value, unit, str, end);
  if (parsedValue.err) return parsedValue;

  ts = StringParser::parse_string(13, 13, parsedValue.next, end);
  if (ts.err) {
    ts = StringParser::parse_string(12, 12, parsedValue.next, end);
    if (ts.err) return ts;
  }

  timestamp = ts.result;
  return ts;
}

ParseResult<void> ValueDecoder::doubleLineTimestampedFixed(
    String& timestamp, uint32_t& value, const char *unit,
    const char *str, const char *end) {
  ParseResult<String> result = StringParser::parse_string(12, 12, str, end);
  if (result.err) return result;
  timestamp = result.result;

  result = StringParser::parse_string(0, 2, result.next, end);
  if (result.err) return result;

  ParseResult<uint32_t> number = NumParser::parse(0, NULL, result.next, end);
  if (number.err) return number;
  number = NumParser::parse(0, NULL, number.next, end);
  if (number.err) return number;

  ParseResult<ObisId> id = ObisIdParser::parse(number.next + 1, end);
  if (id.err) return id;

  const size_t unitSize = strnlen(unit, 3);
  ParseResult<String> parsedUnit =
      StringParser::parse_string(unitSize, unitSize, id.next + 1, end);
  if (parsedUnit.err) return parsedUnit;
  if (memcmp(parsedUnit.result.c_str(), unit, unitSize) != 0)
    return parsedUnit.fail(
        (const __FlashStringHelper*)INVALID_UNIT, id.next + 1);

  const char *start = parsedUnit.next;
  if (*start == '\r') ++start;
  if (*start == '\n') ++start;

  const char *newend = start;
  while (newend != end && *newend != '\r' && *newend != '\n')
    ++newend;

  number = NumParser::parse(3, NULL, start, newend);
  if (!number.err)
    value = number.result;
  return number;
}

ParseResult<void> ValueDecoder::raw(
    String& value, const char *str, const char *end) {
  concat_hack(value, str, end - str);
  return ParseResult<void>().until(end);
}

// Since C++11 it is possible to define the initial values for static
// const members in the class declaration, but if their address is
// taken, they still need a normal definition somewhere (to allocate
// storage).
constexpr char units::none[];
constexpr char units::kWh[];
constexpr char units::Wh[];
constexpr char units::kVArh[];
constexpr char units::VArh[];
constexpr char units::kVAr[];
constexpr char units::VAr[];
constexpr char units::kW[];
constexpr char units::W[];
constexpr char units::V[];
constexpr char units::mV[];
constexpr char units::A[];
constexpr char units::mA[];
constexpr char units::m3[];
constexpr char units::dm3[];
constexpr char units::GJ[];
constexpr char units::MJ[];

/*
  removed 3e lines as of https://github.com/matthijskooijman/arduino-dsmr/issues/36
*/

constexpr ObisId identification::id;
constexpr char identification::name_progmem[];
//constexpr const __FlashStringHelper *identification::name;

constexpr ObisId p1_version::id;
constexpr char p1_version::name_progmem[];
//constexpr const __FlashStringHelper *p1_version::name;

constexpr ObisId p1_version_be::id;
constexpr char p1_version_be::name_progmem[];
//constexpr const __FlashStringHelper *p1_version_be::name;

constexpr ObisId peak_pwr_last_q::id;
constexpr char peak_pwr_last_q::name_progmem[];
//constexpr const __FlashStringHelper *peak_pwr_last_q::name;

constexpr ObisId highest_peak_pwr::id;
constexpr char highest_peak_pwr::name_progmem[];
//constexpr const __FlashStringHelper *highest_peak_pwr::name;

constexpr ObisId highest_peak_pwr_13mnd::id;
constexpr char highest_peak_pwr_13mnd::name_progmem[];

constexpr ObisId timestamp::id;
constexpr char timestamp::name_progmem[];
//constexpr const __FlashStringHelper *timestamp::name;

constexpr ObisId equipment_id::id;
constexpr char equipment_id::name_progmem[];
//constexpr const __FlashStringHelper *equipment_id::name;

constexpr ObisId energy_delivered_total::id;
constexpr char energy_delivered_total::name_progmem[];
//constexpr const __FlashStringHelper *energy_delivered_total::name;

constexpr ObisId energy_returned_total::id;
constexpr char energy_returned_total::name_progmem[];
//constexpr const __FlashStringHelper *energy_returned_total::name;

constexpr ObisId energy_delivered_tariff1::id;
constexpr char energy_delivered_tariff1::name_progmem[];
//constexpr const __FlashStringHelper *energy_delivered_tariff1::name;

constexpr ObisId energy_delivered_tariff2::id;
constexpr char energy_delivered_tariff2::name_progmem[];
//constexpr const __FlashStringHelper *energy_delivered_tariff2::name;

constexpr ObisId energy_returned_tariff1::id;
constexpr char energy_returned_tariff1::name_progmem[];
//constexpr const __FlashStringHelper *energy_returned_tariff1::name;

constexpr ObisId energy_returned_tariff2::id;
constexpr char energy_returned_tariff2::name_progmem[];
//constexpr const __FlashStringHelper *energy_returned_tariff2::name;

constexpr ObisId electricity_tariff::id;
constexpr char electricity_tariff::name_progmem[];
//constexpr const __FlashStringHelper *electricity_tariff::name;

constexpr ObisId power_delivered::id;
constexpr char power_delivered::name_progmem[];
//constexpr const __FlashStringHelper *power_delivered::name;

constexpr ObisId power_returned::id;
constexpr char power_returned::name_progmem[];
//constexpr const __FlashStringHelper *power_returned::name;

constexpr ObisId electricity_threshold::id;
constexpr char electricity_threshold::name_progmem[];
//constexpr const __FlashStringHelper *electricity_threshold::name;

constexpr ObisId electricity_switch_position::id;
constexpr char electricity_switch_position::name_progmem[];
//constexpr const __FlashStringHelper *electricity_switch_position::name;

constexpr ObisId electricity_failures::id;
constexpr char electricity_failures::name_progmem[];
//constexpr const __FlashStringHelper *electricity_failures::name;

constexpr ObisId electricity_long_failures::id;
constexpr char electricity_long_failures::name_progmem[];
//constexpr const __FlashStringHelper *electricity_long_failures::name;

constexpr ObisId electricity_failure_log::id;
constexpr char electricity_failure_log::name_progmem[];
//constexpr const __FlashStringHelper *electricity_failure_log::name;

constexpr ObisId electricity_sags_l1::id;
constexpr char electricity_sags_l1::name_progmem[];
//constexpr const __FlashStringHelper *electricity_sags_l1::name;

constexpr ObisId electricity_sags_l2::id;
constexpr char electricity_sags_l2::name_progmem[];
//constexpr const __FlashStringHelper *electricity_sags_l2::name;

constexpr ObisId electricity_sags_l3::id;
constexpr char electricity_sags_l3::name_progmem[];
//constexpr const __FlashStringHelper *electricity_sags_l3::name;

constexpr ObisId electricity_swells_l1::id;
constexpr char electricity_swells_l1::name_progmem[];
//constexpr const __FlashStringHelper *electricity_swells_l1::name;

constexpr ObisId electricity_swells_l2::id;
constexpr char electricity_swells_l2::name_progmem[];
//constexpr const __FlashStringHelper *electricity_swells_l2::name;

constexpr ObisId electricity_swells_l3::id;
constexpr char electricity_swells_l3::name_progmem[];
//constexpr const __FlashStringHelper *electricity_swells_l3::name;

constexpr ObisId message_short::id;
constexpr char message_short::name_progmem[];
//constexpr const __FlashStringHelper *message_short::name;

constexpr ObisId message_long::id;
constexpr char message_long::name_progmem[];
//constexpr const __FlashStringHelper *message_long::name;

constexpr ObisId voltage_l1::id;
constexpr char voltage_l1::name_progmem[];
//constexpr const __FlashStringHelper *voltage_l1::name;

constexpr ObisId voltage_l2::id;
constexpr char voltage_l2::name_progmem[];
//constexpr const __FlashStringHelper *voltage_l2::name;

constexpr ObisId voltage_l3::id;
constexpr char voltage_l3::name_progmem[];
//constexpr const __FlashStringHelper *voltage_l3::name;

constexpr ObisId current_l1::id;
constexpr char current_l1::name_progmem[];
//constexpr const __FlashStringHelper *current_l1::name;

constexpr ObisId current_l2::id;
constexpr char current_l2::name_progmem[];
//constexpr const __FlashStringHelper *current_l2::name;

constexpr ObisId current_l3::id;
constexpr char current_l3::name_progmem[];
//constexpr const __FlashStringHelper *current_l3::name;

constexpr ObisId power_delivered_l1::id;
constexpr char power_delivered_l1::name_progmem[];
//constexpr const __FlashStringHelper *power_delivered_l1::name;

constexpr ObisId power_delivered_l2::id;
constexpr char power_delivered_l2::name_progmem[];
//constexpr const __FlashStringHelper *power_delivered_l2::name;

constexpr ObisId power_delivered_l3::id;
constexpr char power_delivered_l3::name_progmem[];
//constexpr const __FlashStringHelper *power_delivered_l3::name;

constexpr ObisId power_returned_l1::id;
constexpr char power_returned_l1::name_progmem[];
//constexpr const __FlashStringHelper *power_returned_l1::name;

constexpr ObisId power_returned_l2::id;
constexpr char power_returned_l2::name_progmem[];
//constexpr const __FlashStringHelper *power_returned_l2::name;

constexpr ObisId power_returned_l3::id;
constexpr char power_returned_l3::name_progmem[];
//constexpr const __FlashStringHelper *power_returned_l3::name;

constexpr ObisId mbus1_device_type::id;
constexpr char mbus1_device_type::name_progmem[];
//constexpr const __FlashStringHelper *mbus1_device_type::name;

constexpr ObisId mbus1_equipment_id_tc::id;
constexpr char mbus1_equipment_id_tc::name_progmem[];
//constexpr const __FlashStringHelper *mbus1_equipment_id_tc::name;

constexpr ObisId mbus1_equipment_id_ntc::id;
constexpr char mbus1_equipment_id_ntc::name_progmem[];
//constexpr const __FlashStringHelper *mbus1_equipment_id_ntc::name;

constexpr ObisId mbus1_valve_position::id;
constexpr char mbus1_valve_position::name_progmem[];
//constexpr const __FlashStringHelper *mbus1_valve_position::name;

constexpr ObisId mbus1_delivered::id;
constexpr char mbus1_delivered::name_progmem[];
//constexpr const __FlashStringHelper *mbus1_delivered::name;

constexpr ObisId mbus1_delivered_ntc::id;
constexpr char mbus1_delivered_ntc::name_progmem[];
//constexpr const __FlashStringHelper *mbus1_delivered_ntc::name;

constexpr ObisId mbus1_delivered_dbl::id;
constexpr char mbus1_delivered_dbl::name_progmem[];
//constexpr const __FlashStringHelper *mbus1_delivered_dbl::name;

constexpr ObisId mbus2_device_type::id;
constexpr char mbus2_device_type::name_progmem[];
//constexpr const __FlashStringHelper *mbus2_device_type::name;

constexpr ObisId mbus2_equipment_id_tc::id;
constexpr char mbus2_equipment_id_tc::name_progmem[];
//constexpr const __FlashStringHelper *mbus2_equipment_id_tc::name;

constexpr ObisId mbus2_equipment_id_ntc::id;
constexpr char mbus2_equipment_id_ntc::name_progmem[];
//constexpr const __FlashStringHelper *mbus2_equipment_id_ntc::name;

constexpr ObisId mbus2_delivered_dbl::id;
constexpr char mbus2_delivered_dbl::name_progmem[];
//constexpr const __FlashStringHelper *mbus2_delivered_dbl::name;

constexpr ObisId mbus2_valve_position::id;
constexpr char mbus2_valve_position::name_progmem[];
//constexpr const __FlashStringHelper *mbus2_valve_position::name;

constexpr ObisId mbus2_delivered::id;
constexpr char mbus2_delivered::name_progmem[];
//constexpr const __FlashStringHelper *mbus2_delivered::name;

constexpr ObisId mbus2_delivered_ntc::id;
constexpr char mbus2_delivered_ntc::name_progmem[];
//constexpr const __FlashStringHelper *mbus2_delivered_ntc::name;

constexpr ObisId mbus3_device_type::id;
constexpr char mbus3_device_type::name_progmem[];
//constexpr const __FlashStringHelper *mbus3_device_type::name;

constexpr ObisId mbus3_equipment_id_tc::id;
constexpr char mbus3_equipment_id_tc::name_progmem[];
//constexpr const __FlashStringHelper *mbus3_equipment_id_tc::name;

constexpr ObisId mbus3_equipment_id_ntc::id;
constexpr char mbus3_equipment_id_ntc::name_progmem[];
//constexpr const __FlashStringHelper *mbus3_equipment_id_ntc::name;

constexpr ObisId mbus3_valve_position::id;
constexpr char mbus3_valve_position::name_progmem[];
//constexpr const __FlashStringHelper *mbus3_valve_position::name;

constexpr ObisId mbus3_delivered::id;
constexpr char mbus3_delivered::name_progmem[];
//constexpr const __FlashStringHelper *mbus3_delivered::name;

constexpr ObisId mbus3_delivered_ntc::id;
constexpr char mbus3_delivered_ntc::name_progmem[];
//constexpr const __FlashStringHelper *mbus3_delivered_ntc::name;

constexpr ObisId mbus3_delivered_dbl::id;
constexpr char mbus3_delivered_dbl::name_progmem[];
//constexpr const __FlashStringHelper *mbus3_delivered_dbl::name;

constexpr ObisId mbus4_device_type::id;
constexpr char mbus4_device_type::name_progmem[];
//constexpr const __FlashStringHelper *mbus4_device_type::name;

constexpr ObisId mbus4_equipment_id_tc::id;
constexpr char mbus4_equipment_id_tc::name_progmem[];
//constexpr const __FlashStringHelper *mbus4_equipment_id_tc::name;

constexpr ObisId mbus4_equipment_id_ntc::id;
constexpr char mbus4_equipment_id_ntc::name_progmem[];
//constexpr const __FlashStringHelper *mbus4_equipment_id_ntc::name;

constexpr ObisId mbus4_valve_position::id;
constexpr char mbus4_valve_position::name_progmem[];
//constexpr const __FlashStringHelper *mbus4_valve_position::name;

constexpr ObisId mbus4_delivered::id;
constexpr char mbus4_delivered::name_progmem[];
//constexpr const __FlashStringHelper *mbus4_delivered::name;

constexpr ObisId mbus4_delivered_ntc::id;
constexpr char mbus4_delivered_ntc::name_progmem[];
//constexpr const __FlashStringHelper *mbus4_delivered_ntc::name;

constexpr ObisId mbus4_delivered_dbl::id;
constexpr char mbus4_delivered_dbl::name_progmem[];
//constexpr const __FlashStringHelper *mbus4_delivered_dbl::name;

//BE A1 meters
constexpr ObisId reactive_energy_q1::id;
constexpr char reactive_energy_q1::name_progmem[];
//constexpr const __FlashStringHelper *reactive_energy_q1::name;

constexpr ObisId reactive_energy_q2::id;
constexpr char reactive_energy_q2::name_progmem[];
//constexpr const __FlashStringHelper *reactive_energy_q2::name;

constexpr ObisId reactive_energy_q3::id;
constexpr char reactive_energy_q3::name_progmem[];
//constexpr const __FlashStringHelper *reactive_energy_q3::name;

constexpr ObisId reactive_energy_q4::id;
constexpr char reactive_energy_q4::name_progmem[];
//constexpr const __FlashStringHelper *reactive_energy_q4::name;

constexpr ObisId reactive_power_import::id;
constexpr char reactive_power_import::name_progmem[];
//constexpr const __FlashStringHelper *reactive_power_import::name;

constexpr ObisId reactive_power_export::id;
constexpr char reactive_power_export::name_progmem[];
//constexpr const __FlashStringHelper *reactive_power_export::name;

// Totaal reactieve energie
constexpr ObisId reactive_energy_total_import::id;
constexpr char reactive_energy_total_import::name_progmem[];
//constexpr const __FlashStringHelper *reactive_energy_total_import::name;

constexpr ObisId reactive_energy_total_export::id;
constexpr char reactive_energy_total_export::name_progmem[];
//constexpr const __FlashStringHelper *reactive_energy_total_export::name;

// Per-fase reactief vermogen
constexpr ObisId reactive_power_l1_import::id;
constexpr char reactive_power_l1_import::name_progmem[];
//constexpr const __FlashStringHelper *reactive_power_l1_import::name;

constexpr ObisId reactive_power_l1_export::id;
constexpr char reactive_power_l1_export::name_progmem[];
//constexpr const __FlashStringHelper *reactive_power_l1_export::name;

constexpr ObisId reactive_power_l2_import::id;
constexpr char reactive_power_l2_import::name_progmem[];
//constexpr const __FlashStringHelper *reactive_power_l2_import::name;

constexpr ObisId reactive_power_l2_export::id;
constexpr char reactive_power_l2_export::name_progmem[];
//constexpr const __FlashStringHelper *reactive_power_l2_export::name;

constexpr ObisId reactive_power_l3_import::id;
constexpr char reactive_power_l3_import::name_progmem[];
//constexpr const __FlashStringHelper *reactive_power_l3_import::name;

constexpr ObisId reactive_power_l3_export::id;
constexpr char reactive_power_l3_export::name_progmem[];
//constexpr const __FlashStringHelper *reactive_power_l3_export::name;



