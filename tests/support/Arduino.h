#ifndef DSMR_NATIVE_TEST_ARDUINO_H
#define DSMR_NATIVE_TEST_ARDUINO_H

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>

struct __FlashStringHelper {};

#define F(value) reinterpret_cast<const __FlashStringHelper *>(value)
#define PROGMEM
#define OUTPUT 1
#define HIGH 1
#define LOW 0

inline void delay(unsigned long) {}
inline void pinMode(uint8_t, uint8_t) {}
inline void digitalWrite(uint8_t, uint8_t) {}

class String {
 public:
  String() = default;
  String(const char *value) : value_(value ? value : "") {}
  String(const std::string& value) : value_(value) {}
  String(char value) : value_(1, value) {}
  String(const __FlashStringHelper *value)
      : value_(reinterpret_cast<const char *>(value)) {}

  size_t length() const { return value_.length(); }
  const char *c_str() const { return value_.c_str(); }
  bool reserve(size_t value) {
    value_.reserve(value);
    return value_.capacity() >= value;
  }
  void clear() { value_.clear(); }
  char operator[](size_t index) const { return value_[index]; }

  bool concat(const char *value) {
    value_ += value ? value : "";
    return true;
  }
  bool concat(const char *value, size_t length) {
    if (value) value_.append(value, length);
    return true;
  }
  bool concat(char value) {
    value_ += value;
    return true;
  }

  String& operator=(const char *value) {
    value_ = value ? value : "";
    return *this;
  }
  String& operator+=(const char *value) {
    value_ += value ? value : "";
    return *this;
  }
  String& operator+=(const String& value) {
    value_ += value.value_;
    return *this;
  }
  String& operator+=(char value) {
    value_ += value;
    return *this;
  }
  String& operator+=(const __FlashStringHelper *value) {
    value_ += reinterpret_cast<const char *>(value);
    return *this;
  }

  friend String operator+(const String& lhs, const String& rhs) {
    return String(lhs.value_ + rhs.value_);
  }
  friend String operator+(const char *lhs, const String& rhs) {
    return String(std::string(lhs ? lhs : "") + rhs.value_);
  }
  friend String operator+(const String& lhs, const char *rhs) {
    return String(lhs.value_ + std::string(rhs ? rhs : ""));
  }

 private:
  std::string value_;
};

class Stream {
 public:
  virtual ~Stream() = default;
  virtual int available() { return 0; }
  virtual int read() = 0;
  virtual int peek() { return -1; }
};

#endif
