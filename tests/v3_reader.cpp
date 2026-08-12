#include <dsmr3.h>

#include <stdio.h>

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

  return failures == 0 ? 0 : 1;
}

