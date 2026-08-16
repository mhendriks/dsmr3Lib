#include <dsmr3.h>

#include <stdio.h>
#include <string.h>

#include <string>

static int failures = 0;

#define CHECK(condition) do { \
  if (!(condition)) { \
    fprintf(stderr, "%s:%d check failed: %s\n", __FILE__, __LINE__, #condition); \
    ++failures; \
  } \
} while (0)

class MemoryStream : public Stream {
 public:
  explicit MemoryStream(const std::string& input) : input_(input), position_(0) {}

  int available() override {
    return static_cast<int>(input_.size() - position_);
  }

  int read() override {
    if (position_ == input_.size()) return -1;
    return static_cast<unsigned char>(input_[position_++]);
  }

  int peek() override {
    if (position_ == input_.size()) return -1;
    return static_cast<unsigned char>(input_[position_]);
  }

 private:
  std::string input_;
  size_t position_;
};

static std::string withCrc(const std::string& body) {
  uint16_t crc = 0;
  for (size_t i = 0; i < body.size(); ++i)
    crc = _crc16_update(crc, static_cast<uint8_t>(body[i]));
  char suffix[5];
  snprintf(suffix, sizeof(suffix), "%04X", crc);
  return body + suffix;
}

static P1PacketAccumulator::Result feedAccumulator(const std::string& input) {
  char buffer[128];
  P1PacketAccumulator accumulator(buffer, sizeof(buffer));
  P1PacketAccumulator::Result result = P1PacketAccumulator::Result::WAITING;
  for (size_t i = 0; i < input.size(); ++i) {
    result = accumulator.process(static_cast<uint8_t>(input[i]));
    if (result != P1PacketAccumulator::Result::WAITING)
      return result;
  }
  return result;
}

static P1PacketAccumulator::Result feedPacket(
    P1PacketAccumulator& accumulator, const std::string& input) {
  P1PacketAccumulator::Result result = P1PacketAccumulator::Result::WAITING;
  for (size_t i = 0; i < input.size(); ++i) {
    result = accumulator.process(static_cast<uint8_t>(input[i]));
    if (result != P1PacketAccumulator::Result::WAITING)
      return result;
  }
  return result;
}

static bool readAvailable(const std::string& input) {
  MemoryStream stream(input);
  P1Reader reader(&stream, 1);
  reader.enable(false);
  reader.loop();
  return reader.available();
}

int main() {
  const std::string body = "/TEST\r\n1-0:1.7.0(00.123*kW)\r\n!";

  CHECK(feedAccumulator(body + "\r\n") ==
        P1PacketAccumulator::Result::COMPLETE);
  CHECK(feedAccumulator(withCrc(body)) ==
        P1PacketAccumulator::Result::COMPLETE);

  std::string invalid = withCrc(body);
  invalid[invalid.size() - 1] = invalid.back() == '0' ? '1' : '0';
  CHECK(feedAccumulator(invalid) ==
        P1PacketAccumulator::Result::CHECKSUM_MISMATCH);
  CHECK(feedAccumulator(body + "12X4") ==
        P1PacketAccumulator::Result::MALFORMED_CHECKSUM);

  CHECK(readAvailable(body + "\r\n"));
  CHECK(readAvailable(withCrc(body)));
  CHECK(!readAvailable(invalid));

  {
    char buffer[128];
    P1PacketAccumulator accumulator(buffer, sizeof(buffer));
    CHECK(feedPacket(accumulator, body + "\r\n") ==
          P1PacketAccumulator::Result::COMPLETE);
    CHECK(feedPacket(accumulator, body + "\r\n") ==
          P1PacketAccumulator::Result::COMPLETE);
    CHECK(accumulator.diagnostics().crc_mode == P1CrcMode::DETECTING);
    CHECK(feedPacket(accumulator, body + "\r\n") ==
          P1PacketAccumulator::Result::COMPLETE);
    CHECK(accumulator.diagnostics().crc_mode == P1CrcMode::ABSENT);
    CHECK(accumulator.diagnostics().crc_errors == 0);

    CHECK(feedPacket(accumulator, withCrc(body)) ==
          P1PacketAccumulator::Result::MALFORMED_CHECKSUM);
    CHECK(accumulator.diagnostics().crc_errors == 1);

    accumulator.doChecksum(true);
    CHECK(accumulator.diagnostics().crc_mode == P1CrcMode::DETECTING);
    CHECK(accumulator.diagnostics().crc_errors == 0);
  }

  {
    char buffer[128];
    P1PacketAccumulator accumulator(buffer, sizeof(buffer));
    CHECK(feedPacket(accumulator, withCrc(body)) ==
          P1PacketAccumulator::Result::COMPLETE);
    CHECK(feedPacket(accumulator, body + "\r\n") ==
          P1PacketAccumulator::Result::COMPLETE);
    CHECK(feedPacket(accumulator, withCrc(body)) ==
          P1PacketAccumulator::Result::COMPLETE);
    CHECK(accumulator.diagnostics().crc_mode == P1CrcMode::PRESENT);

    CHECK(feedPacket(accumulator, body + "\r\n") ==
          P1PacketAccumulator::Result::MALFORMED_CHECKSUM);
    CHECK(accumulator.diagnostics().crc_errors == 1);
  }

  {
    MemoryStream stream(
        body + "\r\n" + body + "\r\n" + body + "\r\n");
    P1Reader reader(&stream, 1);
    reader.enable(false);
    for (uint8_t i = 0; i < 3; ++i) {
      CHECK(reader.loop());
      CHECK(reader.available());
      reader.clear();
    }
    CHECK(reader.diagnostics().crc_mode == P1CrcMode::ABSENT);
    CHECK(reader.diagnostics().crc_errors == 0);
  }

  {
    const std::string invalidField =
        "/TEST\r\n"
        "1-0:32.7.0(not-a-number*V)\r\n"
        "1-0:1.7.0(00.123*kW)\r\n"
        "!\r\n";
    MemoryStream stream(invalidField);
    P1FixedReader<256> reader(&stream, 1);
    reader.enable(false);
    CHECK(reader.loop());
    CHECK(reader.available());
    String completeRaw;
    CHECK(reader.CompleteRaw(completeRaw));
    CHECK(strchr(completeRaw.c_str(), '!') != NULL);

    ParsedData<voltage_l1, power_delivered> data;
    String error;
    P1FieldWarning warning;
    CHECK(reader.parse(&data, &error, false, &warning));
    CHECK(reader.diagnostics().skipped_fields == 1);
    CHECK(warning.count == 1);
    CHECK(std::string(warning.line.c_str()) == "1-0:32.7.0(not-a-number*V)");
    CHECK(strchr(completeRaw.c_str(), '!') != NULL);
  }

  {
    const std::string finalFieldBeforeBang =
        "/ADN9 6534\r\n"
        "1-0:71.7.0(0001.4*A)!\r\n";
    MemoryStream stream(finalFieldBeforeBang);
    P1FixedReader<256> reader(&stream, 1);
    reader.enable(false);
    CHECK(reader.loop());
    CHECK(reader.available());

    ParsedData<current_l3> data;
    String error;
    P1FieldWarning warning;
    CHECK(reader.parse(&data, &error, false, &warning));
    CHECK(data.current_l3_present);
    CHECK(data.current_l3.int_val() == 1400);
    CHECK(reader.diagnostics().skipped_fields == 0);
    CHECK(warning.count == 0);
  }

  return failures == 0 ? 0 : 1;
}
