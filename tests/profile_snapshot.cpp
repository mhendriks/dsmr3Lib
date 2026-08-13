#ifndef DSMR_TEST_PUBLIC_HEADER
#define DSMR_TEST_PUBLIC_HEADER "dsmr3.h"
#endif

#include DSMR_TEST_PUBLIC_HEADER

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <type_traits>

#define DSMR_BENCH_PROFILE 2
#include "../benchmarks/field_profiles.h"
#include "fixtures/p1_generator_telegrams.h"

using ProfileData = ParsedData<DSMR_BENCH_P1_DONGLE_FIELDS>;

class SnapshotHash {
 public:
  SnapshotHash() : value_(UINT64_C(1469598103934665603)) {}

  template<typename Field>
  void apply(Field& field) {
    addBytes(&Field::id, sizeof(Field::id));
    const uint8_t present = field.present() ? 1 : 0;
    addBytes(&present, sizeof(present));
    if (present)
      addValue(field.val());
  }

  uint64_t value() const { return value_; }

 private:
  void addBytes(const void *data, size_t size) {
    const uint8_t *bytes = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < size; ++i) {
      value_ ^= bytes[i];
      value_ *= UINT64_C(1099511628211);
    }
  }

  void addValue(String& value) {
    addBytes(value.c_str(), value.length());
  }

  void addValue(FixedValue& value) {
    const uint32_t fixed = value.int_val();
    addBytes(&fixed, sizeof(fixed));
  }

  void addValue(TimestampedFixedValue& value) {
    addValue(static_cast<FixedValue&>(value));
    addValue(value.timestamp);
  }

  template<typename T>
  typename std::enable_if<std::is_integral<T>::value>::type addValue(T& value) {
    addBytes(&value, sizeof(value));
  }

  uint64_t value_;
};

int main() {
  using namespace p1_generator_fixtures;
  printf("fixtures=%lu\n", static_cast<unsigned long>(count));

  for (size_t i = 0; i < count; ++i) {
    ProfileData data;
    ParseResult<void> result = P1Parser::parse(
        &data, all[i].telegram, strlen(all[i].telegram), false, false);
    SnapshotHash snapshot;
    data.applyEach(snapshot);
    printf("case|%s|%u|%016llx\n", all[i].name, result.err ? 0U : 1U,
           static_cast<unsigned long long>(snapshot.value()));
  }
}
