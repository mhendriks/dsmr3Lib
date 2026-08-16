/**
 * Arduino DSMR parser.
 *
 * This software is licensed under the MIT License.
 *
 * Copyright (c) 2015 Matthijs Kooijman <matthijs@stdin.nl>
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
 * P1 reader, that takes care of toggling a request pin, reading data
 * from a serial port and parsing it.
 */

#ifndef DSMR3_INCLUDE_READER_H
#define DSMR3_INCLUDE_READER_H

#include <Arduino.h>
#include "crc16.h"

#include "parser3.h"

namespace dsmr {

enum class P1CrcMode : uint8_t {
  DETECTING,
  PRESENT,
  ABSENT,
};

struct P1Diagnostics {
  P1CrcMode crc_mode;
  uint32_t crc_errors;
  uint32_t skipped_fields;
};

struct P1FieldWarning {
  uint16_t count = 0;
  String line;
};

static void captureFieldWarning(P1FieldWarning *warning,
                                const ParseResult<void>& result,
                                const char *data_end) {
  if (!warning)
    return;

  warning->count = result.field_errors;
  warning->line = "";
  if (!result.field_errors || !result.first_field_line)
    return;

  const char *line_end = result.first_field_line;
  while (line_end < data_end && *line_end != '\r' && *line_end != '\n')
    ++line_end;
  concat_hack(warning->line, result.first_field_line,
              line_end - result.first_field_line);
}

class P1DiagnosticsTracker {
  public:
    P1DiagnosticsTracker() {
      reset();
    }

    void reset() {
      values.crc_mode = P1CrcMode::DETECTING;
      values.crc_errors = 0;
      values.skipped_fields = 0;
      crc_present_votes = 0;
      crc_absent_votes = 0;
    }

    P1Diagnostics diagnostics() const {
      return values;
    }

    P1CrcMode crcMode() const {
      return values.crc_mode;
    }

    void addSkippedFields(uint16_t count) {
      values.skipped_fields += count;
    }

    void recordCrcError() {
      ++values.crc_errors;
    }

    void observeCrc(bool present) {
      if (values.crc_mode != P1CrcMode::DETECTING)
        return;

      if (present)
        ++crc_present_votes;
      else
        ++crc_absent_votes;

      if ((uint8_t)(crc_present_votes + crc_absent_votes) >= 3) {
        values.crc_mode = crc_present_votes >= 2
            ? P1CrcMode::PRESENT
            : P1CrcMode::ABSENT;
        crc_present_votes = 0;
        crc_absent_votes = 0;
      }
    }

  private:
    P1Diagnostics values;
    uint8_t crc_present_votes;
    uint8_t crc_absent_votes;
};

/**
 * Lightweight state machine that accumulates one P1 telegram into a caller
 * supplied fixed-size buffer. The stored data matches P1Reader::raw(): it
 * contains the bytes between '/' and '!', excluding both markers and excluding
 * the checksum.
 */
class P1PacketAccumulator {
  public:
    enum class Result : uint8_t {
      WAITING,
      COMPLETE,
      CHECKSUM_MISMATCH,
      MALFORMED_CHECKSUM,
      BUFFER_OVERFLOW,
    };

    P1PacketAccumulator(char *buffer, size_t capacity, bool checksum = true)
      : buffer(buffer), capacity(capacity), state(State::WAITING_STATE),
        length(0), crc(0), crc_pos(0), last_crc_present(false) {
      (void)checksum;
      reset();
    }

    void doChecksum(bool checksum) {
      // Kept for dsmr2Lib source compatibility. V3 auto-detects CRC.
      (void)checksum;
      resetDiagnostics();
    }

    void resetDiagnostics() {
      diagnostics_tracker.reset();
      reset();
    }

    P1Diagnostics diagnostics() const {
      return diagnostics_tracker.diagnostics();
    }

    void addSkippedFields(uint16_t count) {
      diagnostics_tracker.addSkippedFields(count);
    }

    bool lastCrcPresent() const {
      return last_crc_present;
    }

    Result process(uint8_t c) {
      switch (state) {
        case State::WAITING_STATE:
          if (c == '/') {
            start();
          }
          return Result::WAITING;

        case State::READING_STATE:
          if (diagnostics_tracker.crcMode() != P1CrcMode::ABSENT)
            crc = _crc16_update(crc, c);
          if (c == '!') {
            state = State::CHECKSUM_STATE;
            crc_pos = 0;
            return Result::WAITING;
          }

          if (length + 1 >= capacity) {
            reset();
            return Result::BUFFER_OVERFLOW;
          }

          buffer[length++] = (char)c;
          buffer[length] = '\0';
          return Result::WAITING;

        case State::CHECKSUM_STATE:
          if ((c == '\r' || c == '\n') && crc_pos == 0) {
            if (diagnostics_tracker.crcMode() == P1CrcMode::PRESENT) {
              recordCrcError();
              return Result::MALFORMED_CHECKSUM;
            }
            last_crc_present = false;
            diagnostics_tracker.observeCrc(false);
            state = State::WAITING_STATE;
            return Result::COMPLETE;
          }
          if (diagnostics_tracker.crcMode() == P1CrcMode::ABSENT) {
            recordCrcError();
            return Result::MALFORMED_CHECKSUM;
          }
          if (!isxdigit(c)) {
            recordCrcError();
            return Result::MALFORMED_CHECKSUM;
          }
          crc_buf[crc_pos++] = (char)c;
          if (crc_pos < CrcParser::CRC_LEN)
            return Result::WAITING;

          state = State::WAITING_STATE;
          crc_buf[CrcParser::CRC_LEN] = '\0';
          {
            ParseResult<uint16_t> parsed = CrcParser::parse(crc_buf, crc_buf + CrcParser::CRC_LEN);
            if (parsed.err) {
              recordCrcError();
              return Result::MALFORMED_CHECKSUM;
            }
            if (parsed.result != crc) {
              recordCrcError();
              return Result::CHECKSUM_MISMATCH;
            }
          }
          last_crc_present = true;
          diagnostics_tracker.observeCrc(true);
          return Result::COMPLETE;
      }

      return Result::WAITING;
    }

    void reset() {
      state = State::WAITING_STATE;
      length = 0;
      crc = 0;
      crc_pos = 0;
      last_crc_present = false;
      if (capacity)
        buffer[0] = '\0';
    }

    const char *data() const {
      return buffer;
    }

    size_t size() const {
      return length;
    }

    uint16_t GetCRC() const {
      return crc;
    }

  protected:
    enum class State : uint8_t {
      WAITING_STATE,
      READING_STATE,
      CHECKSUM_STATE,
    };

    void start() {
      state = State::READING_STATE;
      length = 0;
      crc = diagnostics_tracker.crcMode() == P1CrcMode::ABSENT
          ? 0
          : _crc16_update(0, '/');
      crc_pos = 0;
      last_crc_present = false;
      if (capacity)
        buffer[0] = '\0';
    }

    void recordCrcError() {
      diagnostics_tracker.recordCrcError();
      reset();
    }

    char *buffer;
    size_t capacity;
    State state;
    size_t length;
    uint16_t crc;
    char crc_buf[CrcParser::CRC_LEN + 1];
    uint8_t crc_pos;
    P1DiagnosticsTracker diagnostics_tracker;
    bool last_crc_present;
};

template<size_t MaxTelegramLength>
class P1FixedReader {
  public:
    P1FixedReader(Stream *stream, uint8_t req_pin, bool checksum = true)
      : stream(stream), req_pin(req_pin), once(false), state(State::DISABLED_STATE),
        _available(false), accumulator(buffer, lengthof(buffer), checksum) {
      pinMode(req_pin, OUTPUT);
      digitalWrite(req_pin, LOW);
    }

    void doChecksum(bool checksum) {
      accumulator.doChecksum(checksum);
      _available = false;
    }

    void enable(bool once) {
      digitalWrite(this->req_pin, HIGH);
      this->state = State::WAITING_STATE;
      this->once = once;
    }

    void disable() {
      digitalWrite(this->req_pin, LOW);
      this->state = State::DISABLED_STATE;
      if (!_available)
        accumulator.reset();
      while(this->stream->read() >= 0) /* nothing */;
    }

    bool available() const {
      return _available;
    }

    bool loop() {
      while (true) {
        int c = this->stream->read();
        if (c < 0)
          return _available;

        if (state == State::DISABLED_STATE)
          continue;

        P1PacketAccumulator::Result res = accumulator.process((uint8_t)c);
        if (res == P1PacketAccumulator::Result::COMPLETE) {
          _available = true;
          if (once)
            disable();
          return true;
        }

        if (res == P1PacketAccumulator::Result::BUFFER_OVERFLOW ||
            res == P1PacketAccumulator::Result::CHECKSUM_MISMATCH ||
            res == P1PacketAccumulator::Result::MALFORMED_CHECKSUM) {
          _available = false;
        }

        delay(0);
      }
    }

    const char *raw() const {
      return accumulator.data();
    }

    size_t rawLength() const {
      return accumulator.size();
    }

    uint16_t GetCRC() const {
      return accumulator.GetCRC();
    }

    bool CompleteRaw(String& destination) const {
      const size_t length = rawLength();
      if (length == 0)
        return false;

      const size_t result_length = length + 2
          + (accumulator.lastCrcPresent() ? 4 : 0);
      if (!destination.reserve(result_length))
        return false;

      destination.clear();
      destination.concat('/');
      destination.concat(raw(), length);
      destination.concat('!');
      if (accumulator.lastCrcPresent()) {
        char crc_str[5];
        snprintf(crc_str, sizeof(crc_str), "%04X", GetCRC());
        destination.concat(crc_str);
      }
      return true;
    }

    String CompleteRaw() const {
      String result;
      CompleteRaw(result);
      return result;
    }

    String GetCRC_str() const {
      char buf[5];
      snprintf(buf, sizeof(buf), "%04X", GetCRC());
      return buf;
    }

    template<typename... Ts>
    bool parse(ParsedData<Ts...> *data, String *err, bool = false,
               P1FieldWarning *field_warning = NULL) {
      const char *str = raw(), *end = raw() + rawLength();
      ParseResult<void> res = P1Parser::parse_data(data, str, end);
      accumulator.addSkippedFields(res.field_errors);

      if (res.err && err)
        *err = res.fullError(str, end);
      captureFieldWarning(field_warning, res, end);

      clear();
      return res.err == NULL;
    }

    P1Diagnostics diagnostics() const {
      return accumulator.diagnostics();
    }

    void clearAll() {
      accumulator.reset();
      _available = false;
    }

    void clear() {
      if (_available) {
        accumulator.reset();
        _available = false;
      }
    }

    void ChangeStream(Stream *new_stream) {
      stream = new_stream;
      accumulator.resetDiagnostics();
      _available = false;
    }

  protected:
    Stream *stream;
    uint8_t req_pin;
    enum class State : uint8_t {
      DISABLED_STATE,
      WAITING_STATE,
    };
    bool once;
    State state;
    bool _available;
    char buffer[MaxTelegramLength + 1];
    P1PacketAccumulator accumulator;
};

/**
 * Controls the request pin on the P1 port to enable (periodic)
 * transmission of messages and reads those messages.
 *
 * To enable the request pin, call enable(). This lets the Smart Meter
 * start periodically sending messages. While the request pin is
 * enabled, loop() should be regularly called to read pending bytes.
 *
 * Once a full and correct message is received, loop() (and available())
 * start returning true, until the message is cleared. You can then
 * either read the raw message using raw(), or parse it using parse().
 *
 * The message is cleared when:
 *  - clear() is called
 *  - parse() is called
 *  - loop() is called and the start of a new message is available
 *
 * When disable is called, the request pin is disabled again and any
 * partial message is discarded. Any bytes received while disabled are
 * dropped.
 */
class P1Reader {
  public:
    /**
     * Create a new P1Reader. The stream passed should be the serial
     * port to which the P1 TX pin is connected. The req_pin is the
     * pin connected to the request pin. The pin is configured as an
     * output, the Stream is assumed to be already set up (e.g. baud
     * rate configured).
     */
    P1Reader(Stream *stream, uint8_t req_pin, bool checksum = true)
      : stream(stream), req_pin(req_pin), _available(false), once(false),
        state(State::DISABLED_STATE), crc(0), last_crc_present(false) {
      pinMode(req_pin, OUTPUT);
      //digitalWrite(req_pin, HIGH);
      digitalWrite(req_pin, LOW);
      (void)checksum;
    }

    /**
     * Retained for source compatibility. V3 detects an optional CRC
     * automatically and always validates it when present.
     */
    void doChecksum(bool checksum) {
      // Kept for dsmr2Lib source compatibility. V3 auto-detects CRC.
      (void)checksum;
      resetDiagnostics();
    }

    void resetDiagnostics() {
      diagnostics_tracker.reset();
      last_crc_present = false;
      clearAll();
    }

    P1Diagnostics diagnostics() const {
      return diagnostics_tracker.diagnostics();
    }

    /**
     * Enable the request pin, to request data on the P1 port.
     * @param  once    When true, the request pin is automatically
     *                 disabled once a complete and correct message was
     *                 receivedc. When false, the request pin stays
     *                 enabled, so messages will continue to be sent
     *                 periodically.
     */
    void enable(bool once) {
//      digitalWrite(this->req_pin, LOW);
      digitalWrite(this->req_pin, HIGH);
      this->state = State::WAITING_STATE;
      this->once = once;
    }

    /* Disable the request pin again, to stop data from being sent on
     * the P1 port. This will also clear any incomplete data that was
     * previously received, but a complete message will be kept until
     * clear() is called.
     */
    void disable() {
      //digitalWrite(this->req_pin, HIGH);
	  digitalWrite(this->req_pin, LOW);
      this->state = State::DISABLED_STATE;
      if (!this->_available)
        this->buffer = "";
      // Clear any pending bytes
      while(this->stream->read() >= 0) /* nothing */;
    }

    /**
     * Returns true when a complete and correct message was received,
     * until it is cleared.
     */
    bool available() {
      return this->_available;
    }
    
    uint16_t GetCRC() {
      return this->crc;
    }
    
    bool CompleteRaw(String& destination) const {
        const size_t length = buffer.length();
        if (length == 0) return false;

        const size_t result_length = length + 2 + (last_crc_present ? 4 : 0);
        if (!destination.reserve(result_length)) return false;

        destination.clear();
        destination.concat('/');
        destination.concat(buffer.c_str(), length);
        destination.concat('!');
        if (last_crc_present) {
          char crc_str[5];
          snprintf(crc_str, sizeof(crc_str), "%04X", this->crc);
          destination.concat(crc_str);
        }
        return true;
    }

    String CompleteRaw() const {
        String result;
        CompleteRaw(result);
        return result;
    }
    
    String GetCRC_str() {
      char buf[5];
      snprintf(buf, sizeof(buf), "%04X", this->crc);
      return buf;
    }
    
    /**
     * Check for new data to read. Should be called regularly, such as
     * once every loop. Returns true if a complete message is available
     * (just like available).
     */
    bool loop() {
      while(true) {
        if (state == State::CHECKSUM_STATE) {
          if (this->stream->available() < 1)
            return false;

          char buf[CrcParser::CRC_LEN];
          const int suffix_start = this->stream->peek();
          bool valid = false;
          if (suffix_start == '\r' || suffix_start == '\n') {
            // No CRC suffix: the line ending terminates the telegram.
            this->stream->read();
            if (diagnostics_tracker.crcMode() == P1CrcMode::PRESENT) {
              recordCrcError();
            } else {
              last_crc_present = false;
              diagnostics_tracker.observeCrc(false);
              valid = true;
            }
          } else {
            if (diagnostics_tracker.crcMode() == P1CrcMode::ABSENT) {
              recordCrcError();
              return false;
            }
            if (!isxdigit((unsigned char)suffix_start)) {
              recordCrcError();
              return false;
            }
            if ((size_t)this->stream->available() < CrcParser::CRC_LEN)
              return false;
            for (uint8_t i = 0; i < CrcParser::CRC_LEN; ++i) {
              delay(0);
              buf[i] = this->stream->read();
            }
            ParseResult<uint16_t> parsed = CrcParser::parse(buf, buf + lengthof(buf));
            valid = !parsed.err && parsed.result == this->crc;
            if (valid) {
              last_crc_present = true;
              diagnostics_tracker.observeCrc(true);
            } else {
              recordCrcError();
            }
          }

          /*
           * Prepare for the next message only after the optional suffix has
           * been classified. A present but invalid CRC never becomes
           * available.
           */
          state = State::WAITING_STATE;
          this->_available = valid;
          if (once && valid)
            this->disable();
          return true;

        } else {
          // For other states, read bytes one by one
          int c = this->stream->read();
          if (c < 0)
            return false;

          switch (this->state) {
            case State::DISABLED_STATE:
              // Where did this byte come from? Just toss it
              break;
            case State::WAITING_STATE:
              if (c == '/') {
                this->state = State::READING_STATE;
                // Include the / in the CRC
                this->crc = diagnostics_tracker.crcMode() == P1CrcMode::ABSENT
                    ? 0
                    : _crc16_update(0, c);
                last_crc_present = false;
                this->clear();
              }
              break;
            case State::READING_STATE:
              // Include the ! in the CRC
              if (diagnostics_tracker.crcMode() != P1CrcMode::ABSENT)
                this->crc = _crc16_update(this->crc, c);
              if (c == '!')
                this->state = State::CHECKSUM_STATE;
              else
                buffer.concat((char)c);

              break;
            case State::CHECKSUM_STATE:
              // This cannot happen (given the surrounding if), but the
              // compiler is not smart enough to see this, so list this
              // case to prevent a warning.
              abort();
              break;
          }
        }
        delay(0); // force yield()
      }
      return false;
    }

    /**
     * Returns the data read so far.
     */
    const String &raw() {
      return buffer;
    }

    size_t rawLength() const {
      return buffer.length();
    }

    /**
     * If a complete message has been received, parse it and store the
     * result into the ParsedData object passed.
     *
     * After parsing, the message is cleared.
     *
     * If parsing fails, false is returned. If err is passed, the error
     * message is appended to that string.
     */
    template<typename... Ts>
    //--bool parse(ParsedData<Ts...> *data, String *err, bool unknown_error = false) {
    bool parse(ParsedData<Ts...> *data, String *err, bool checksum = false,
               P1FieldWarning *field_warning = NULL) {
      const char *str = buffer.c_str(), *end = buffer.c_str() + buffer.length();
      ParseResult<void> res = P1Parser::parse_data(data, str, end);
      diagnostics_tracker.addSkippedFields(res.field_errors);

      if (res.err && err)
        *err = res.fullError(str, end);
      captureFieldWarning(field_warning, res, end);

      // Clear the message
      this->clear();

      return res.err == NULL;
    }

    /**
     * Clear all data from the buffer.
     */
    void clearAll() {
        buffer = "";
        _available = false;
    }

    /**
     * Clear any complete message from the buffer.
     */
    void clear() {
      if (_available) {
        buffer = "";
        _available = false;
      }
    }
    
    void ChangeStream(Stream *new_stream){
		stream = new_stream;
		resetDiagnostics();
    }

  protected:
    Stream *stream;
    uint8_t req_pin;
    enum class State : uint8_t {
      DISABLED_STATE,
      WAITING_STATE,
      READING_STATE,
      CHECKSUM_STATE,
    };
    bool _available;
    bool once, invert_dtr = false;
    State state;
    String buffer;
    uint16_t crc;
    P1DiagnosticsTracker diagnostics_tracker;
    bool last_crc_present;

    void recordCrcError() {
      diagnostics_tracker.recordCrcError();
      state = State::WAITING_STATE;
      _available = false;
      buffer = "";
      last_crc_present = false;
    }
};

} // namespace dsmr

#endif // DSMR3_INCLUDE_READER_H
