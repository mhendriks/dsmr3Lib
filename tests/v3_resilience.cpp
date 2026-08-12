#include <dsmr3.h>

#include <stdio.h>
#include <string.h>

#include <string>

using TestData = ParsedData<voltage_l1, power_delivered>;

static int failures = 0;

#define CHECK(condition) do { \
  if (!(condition)) { \
    fprintf(stderr, "%s:%d check failed: %s\n", __FILE__, __LINE__, #condition); \
    ++failures; \
  } \
} while (0)

static std::string withCrc(const char *body) {
  uint16_t crc = 0;
  for (const char *cursor = body; *cursor; ++cursor)
    crc = _crc16_update(crc, static_cast<uint8_t>(*cursor));
  char suffix[5];
  snprintf(suffix, sizeof(suffix), "%04X", crc);
  return std::string(body) + suffix;
}

static ParseResult<void> parse(TestData& data, const std::string& telegram) {
  return P1Parser::parse(&data, telegram.c_str(), telegram.size());
}

int main() {
  const char *body =
      "/TEST\r\n"
      "1-0:32.7.0(230.1*V)\r\n"
      "1-0:1.7.0(01.234*kW)\r\n"
      "!";

  {
    TestData data;
    ParseResult<void> result = parse(data, body);
    CHECK(!result.err);
    CHECK(data.voltage_l1_present && data.voltage_l1.int_val() == 230100);
    CHECK(data.power_delivered_present && data.power_delivered.int_val() == 1234);
  }

  {
    TestData data;
    ParseResult<void> result = parse(data, withCrc(body));
    CHECK(!result.err);
    CHECK(result.field_errors == 0);
  }

  {
    TestData data;
    std::string invalid = withCrc(body);
    invalid[invalid.size() - 1] = invalid.back() == '0' ? '1' : '0';
    ParseResult<void> result = parse(data, invalid);
    CHECK(result.err != NULL);
    CHECK(!data.voltage_l1_present);
    CHECK(!data.power_delivered_present);
  }

  {
    TestData data;
    ParseResult<void> result = parse(data, std::string(body) + "12X4");
    CHECK(result.err != NULL);
  }

  {
    const char *invalid_field =
        "/TEST\r\n"
        "1-0:32.7.0(not-a-number*V)\r\n"
        "1-0:1.7.0(01.234*kW)\r\n"
        "!";
    TestData data;
    ParseResult<void> result = parse(data, invalid_field);
    CHECK(!result.err);
    CHECK(result.field_errors == 1);
    CHECK(!data.voltage_l1_present);
    CHECK(data.power_delivered_present && data.power_delivered.int_val() == 1234);
  }

  {
    const char *unknown_and_malformed =
        "/TEST\r\n"
        "this is not an OBIS field\r\n"
        "9-9:9.9.9(value ignored)\r\n"
        "1-0:1.7.0(00.321*kW)\r\n"
        "!";
    TestData data;
    ParseResult<void> result = parse(data, unknown_and_malformed);
    CHECK(!result.err);
    CHECK(result.field_errors == 1);
    CHECK(data.power_delivered_present && data.power_delivered.int_val() == 321);
  }

  return failures == 0 ? 0 : 1;
}

