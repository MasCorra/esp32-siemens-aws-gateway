// Native tests for PLC logic, messages, and timing.
#include "../firmware/PLC_AWS_Gateway/GatewayCore.h"
#include "../firmware/PLC_AWS_Gateway/GatewayProtocol.h"
#include <ArduinoJson.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {
int assertions = 0, failures = 0, tests = 0;
#define CHECK(expression)                                                     \
  do {                                                                      \
    ++assertions;                                                           \
    if (!(expression)) {                                                    \
      ++failures;                                                           \
      std::cerr << __func__ << ':' << __LINE__ << ": " #expression "\n";       \
    }                                                                       \
  } while (false)

using gateway::DateEncoding;
using gateway::DateFormat;
using gateway::Sample;
constexpr char kDate[] = "2024-01-01 01:00:00";

Sample expectedSample() {
  Sample sample;
  std::strcpy(sample.date, kDate);
  sample.spoolFeedback = 1.25F;
  sample.spoolCommand = -2.5F;
  sample.axisPositionFeedback = 3.0F;
  sample.axisPositionReference = 4.0F;
  sample.hydraulicPressure = 5.0F;
  sample.counterPlc = -32768;
  sample.messageCounter = 32767;
  sample.timestamp = 0;
  return sample;
}

std::array<uint8_t, 312> frame() {
  std::array<uint8_t, 312> bytes;
  bytes.fill(0x55);  // Bytes beyond the declared date length are deliberately non-NUL.
  bytes[0] = 254;
  bytes[1] = 19;
  std::copy_n(kDate, 19, bytes.begin() + 2);
  // Independently calculated big-endian IEEE-754 values: 1.25, -2.5, 3, 4, 5.
  const uint8_t realBytes[] = {
      0x3F, 0xA0, 0, 0, 0xC0, 0x20, 0, 0, 0x40, 0x40, 0, 0,
      0x40, 0x80, 0, 0, 0x40, 0xA0, 0, 0};
  std::copy_n(realBytes, 20, bytes.begin() + 256);
  bytes[308] = 0x80;
  bytes[309] = 0;
  bytes[310] = 0x7F;
  bytes[311] = 0xFF;
  return bytes;
}

// Output-preservation checks do not use sameSample, which excludes metadata.
void checkWholeSample(const Sample& actual, const Sample& expected) {
  CHECK(std::memcmp(actual.date, expected.date, gateway::kDateCapacity) == 0);
  CHECK(actual.spoolFeedback == expected.spoolFeedback);
  CHECK(actual.spoolCommand == expected.spoolCommand);
  CHECK(actual.axisPositionFeedback == expected.axisPositionFeedback);
  CHECK(actual.axisPositionReference == expected.axisPositionReference);
  CHECK(actual.hydraulicPressure == expected.hydraulicPressure);
  CHECK(actual.counterPlc == expected.counterPlc);
  CHECK(actual.messageCounter == expected.messageCounter);
  CHECK(actual.timestamp == expected.timestamp);
}

void checkDecoded(const Sample& actual) {
  CHECK(std::strncmp(actual.date, kDate, gateway::kDateCapacity) == 0);
  CHECK(actual.spoolFeedback == 1.25F);
  CHECK(actual.spoolCommand == -2.5F);
  CHECK(actual.axisPositionFeedback == 3.0F);
  CHECK(actual.axisPositionReference == 4.0F);
  CHECK(actual.hydraulicPressure == 5.0F);
  CHECK(actual.counterPlc == -32768);
  CHECK(actual.messageCounter == 32767);
  CHECK(actual.timestamp == 0);
}

void s7FrameUsesAdvancedOffsetsAndDeclaredStringLength() {
  CHECK(gateway::kFrameSize == 312);
  CHECK(gateway::kDateCapacity == 255);
  auto bytes = frame();
  Sample actual;
  actual.timestamp = 123;
  CHECK(gateway::decodeFrame(bytes.data(), bytes.size(), DateEncoding::S7StringHeaderAt0, actual));
  checkDecoded(actual);
  CHECK(actual.date[19] == '\0');
  std::array<uint8_t, 314> extended{};
  std::copy(bytes.begin(), bytes.end(), extended.begin());
  CHECK(gateway::decodeFrame(extended.data(), extended.size(), DateEncoding::S7StringHeaderAt0, actual));
  checkDecoded(actual);
}

void rawStringAtOffsetTwoRequiresExplicitEncoding() {
  auto bytes = frame();
  bytes[0] = 255;  // Invalid S7 header; headerless decoding must ignore these bytes.
  bytes[1] = 255;
  bytes[21] = 0;
  Sample actual;
  CHECK(gateway::decodeFrame(bytes.data(), bytes.size(), DateEncoding::NullTerminatedAt2, actual));
  checkDecoded(actual);
  actual = expectedSample();
  CHECK(!gateway::decodeFrame(bytes.data(), bytes.size(), DateEncoding::S7StringHeaderAt0, actual));
  checkWholeSample(actual, expectedSample());
}

void frameRejectionsPreserveAllFields() {
  auto bytes = frame();
  Sample original = expectedSample();
  original.timestamp = 1704067200;
  for (size_t length : {size_t(0), size_t(2), size_t(255), size_t(275), size_t(308), size_t(311)}) {
    Sample actual = original;
    CHECK(!gateway::decodeFrame(bytes.data(), length, DateEncoding::S7StringHeaderAt0, actual));
    checkWholeSample(actual, original);
  }
  for (DateEncoding encoding : {DateEncoding::Unconfigured, static_cast<DateEncoding>(99)}) {
    Sample actual = original;
    CHECK(!gateway::decodeFrame(bytes.data(), bytes.size(), encoding, actual));
    checkWholeSample(actual, original);
  }
  Sample actual = original;
  CHECK(!gateway::decodeFrame(nullptr, 312, DateEncoding::S7StringHeaderAt0, actual));
  checkWholeSample(actual, original);
}

void s7StringValidatesHeaderAndEmbeddedNul() {
  const Sample original = expectedSample();
  for (int invalidCase = 0; invalidCase < 4; ++invalidCase) {
    auto bytes = frame();
    if (invalidCase == 0) bytes[0] = 255;
    if (invalidCase == 1) bytes[1] = 0;
    if (invalidCase == 2) { bytes[0] = 18; bytes[1] = 19; }
    if (invalidCase == 3) bytes[10] = 0;
    Sample actual = original;
    CHECK(!gateway::decodeFrame(bytes.data(), bytes.size(), DateEncoding::S7StringHeaderAt0, actual));
    checkWholeSample(actual, original);
  }
  auto maximum = frame();
  maximum[0] = 254;
  maximum[1] = 254;
  std::fill(maximum.begin() + 2, maximum.begin() + 256, 'x');
  Sample actual;
  CHECK(gateway::decodeFrame(maximum.data(), maximum.size(), DateEncoding::S7StringHeaderAt0, actual));
  CHECK(std::count(actual.date, actual.date + 254, 'x') == 254);
  CHECK(actual.date[254] == '\0');
  CHECK(actual.spoolFeedback == 1.25F);
}

void rawStringMustTerminateBeforeMetrics() {
  const Sample original = expectedSample();
  for (bool empty : {false, true}) {
    auto bytes = frame();
    if (empty) bytes[2] = 0;
    // Otherwise, bytes 2..255 contain no NUL; zeros in REAL fields do not count.
    Sample actual = original;
    CHECK(!gateway::decodeFrame(bytes.data(), bytes.size(), DateEncoding::NullTerminatedAt2, actual));
    checkWholeSample(actual, original);
  }
  auto bytes = frame();
  std::fill(bytes.begin() + 2, bytes.begin() + 255, 'r');
  bytes[255] = 0;
  Sample actual;
  CHECK(gateway::decodeFrame(bytes.data(), bytes.size(), DateEncoding::NullTerminatedAt2, actual));
  CHECK(std::count(actual.date, actual.date + 253, 'r') == 253);
  CHECK(actual.date[253] == '\0');
  CHECK(actual.spoolFeedback == 1.25F);
}

void everyNonfiniteMetricRejectsWholeFrame() {
  const size_t offsets[] = {256, 260, 264, 268, 272};
  const uint8_t invalid[][4] = {{0x7F, 0x80, 0, 0}, {0xFF, 0x80, 0, 0}, {0x7F, 0xC0, 0, 0}};
  Sample original = expectedSample();
  original.timestamp = 42;
  for (size_t offset : offsets) {
    for (const auto& real : invalid) {
      auto bytes = frame();
      std::copy_n(real, 4, bytes.begin() + offset);
      Sample actual = original;
      CHECK(!gateway::decodeFrame(bytes.data(), bytes.size(), DateEncoding::S7StringHeaderAt0, actual));
      checkWholeSample(actual, original);
    }
  }
}

void equalityTracksFiveMetricsAndIgnoresMetadata() {
  const Sample original = expectedSample();
  CHECK(gateway::sameSample(original, original));
  Sample metadata = original;
  std::strcpy(metadata.date, "2099-12-31 23:59:59");
  metadata.timestamp = 4102444799ULL;
  metadata.counterPlc = 0;
  metadata.messageCounter = -1;
  CHECK(gateway::sameSample(original, metadata));
  float Sample::* metrics[] = {&Sample::spoolFeedback, &Sample::spoolCommand,
      &Sample::axisPositionFeedback, &Sample::axisPositionReference, &Sample::hydraulicPressure};
  for (auto metric : metrics) {
    Sample changed = original;
    changed.*metric += 0.25F;
    CHECK(!gateway::sameSample(original, changed));
    CHECK(!gateway::sameSample(changed, original));
  }
}

void publicationBaselineIncludesHealthButOnlySuccessfulPublications() {
  gateway::PublicationTracker tracker;
  Sample zero;
  CHECK(tracker.pending(zero, false));
  tracker.recordResult(zero, false, false);
  CHECK(tracker.pending(zero, false));
  tracker.recordResult(zero, false, true);
  CHECK(!tracker.pending(zero, false));
  CHECK(tracker.pending(zero, true));
  tracker.recordResult(zero, true, false);
  CHECK(tracker.pending(zero, true));
  CHECK(!tracker.pending(zero, false));
  tracker.recordResult(zero, true, true);
  CHECK(!tracker.pending(zero, true));
  CHECK(tracker.pending(zero, false));
  zero.timestamp = 123;
  zero.counterPlc = 7;
  std::strcpy(zero.date, "changed");
  CHECK(!tracker.pending(zero, true));
  Sample changed = zero;
  changed.axisPositionReference = 1;
  CHECK(tracker.pending(changed, true));
  tracker.recordResult(changed, true, false);
  CHECK(tracker.pending(changed, true));
  CHECK(!tracker.pending(zero, true));
  tracker.recordResult(changed, true, true);
  CHECK(!tracker.pending(changed, true));
}

void intervalGateUsesAttemptTimesAndExactBoundary() {
  gateway::IntervalGate gate(1000);
  CHECK(gate.ready(0));
  CHECK(gate.ready(500));
  gate.mark(500);
  CHECK(!gate.ready(500));
  CHECK(!gate.ready(1499));
  CHECK(gate.ready(1500));
  gate.mark(1500);
  CHECK(!gate.ready(2499));
  CHECK(gate.ready(2500));
  gateway::IntervalGate immediate(0);
  immediate.mark(42);
  CHECK(immediate.ready(42));
}

void intervalGateHandlesMillisRollover() {
  gateway::IntervalGate gate(100);
  gate.mark(0xFFFFFFF0U);
  CHECK(!gate.ready(0xFFFFFFFFU));
  CHECK(!gate.ready(0));
  CHECK(!gate.ready(83));
  CHECK(gate.ready(84));
  gate.mark(84);
  CHECK(!gate.ready(183));
  CHECK(gate.ready(184));
}

void heartbeatRequiresObservationAndActualCounterChange() {
  gateway::HeartbeatMonitor monitor(100);
  CHECK(!monitor.healthy(0));
  monitor.observe(32767, 10);
  CHECK(monitor.healthy(10));
  monitor.observe(32767, 90);  // Repeated reads cannot make a frozen PLC appear healthy.
  CHECK(monitor.healthy(109));
  CHECK(!monitor.healthy(110));
  monitor.observe(-32768, 120);  // Signed counter rollover from maximum to minimum counts as a change.
  CHECK(monitor.healthy(120));
  CHECK(monitor.healthy(219));
  CHECK(!monitor.healthy(220));
  monitor.observe(-123, 230);  // The counter does not need to increase by one.
  CHECK(monitor.healthy(230));
  monitor.invalidate();
  CHECK(!monitor.healthy(231));
  monitor.observe(-123, 240);  // After invalidation, the same value counts as the first observation.
  CHECK(monitor.healthy(240));
}

void heartbeatHandlesMillisRollover() {
  gateway::HeartbeatMonitor monitor(100);
  monitor.observe(-1, 0xFFFFFFF0U);
  CHECK(monitor.healthy(0));
  monitor.observe(-1, 50);
  CHECK(monitor.healthy(83));
  CHECK(!monitor.healthy(84));
  monitor.observe(0, 90);
  CHECK(monitor.healthy(189));
  CHECK(!monitor.healthy(190));
}

void activeLowRelayStartsInactiveUnderEsp32RegistrationRules() {
  bool registered = false, output = false, level = false, everDroveLow = false;
  gateway::initializeActiveLowOutput(15,
      [&](uint8_t pin, bool enableOutput) {
        CHECK(pin == 15);
        registered = true;
        output = enableOutput;
        if (output && !level) everDroveLow = true;
      },
      [&](uint8_t pin, bool high) {
        CHECK(pin == 15);
        if (registered) level = high;  // ESP32 ignores writes to unregistered pins.
        if (output && !level) everDroveLow = true;
      });
  CHECK(registered);
  CHECK(output);
  CHECK(level);
  CHECK(!everDroveLow);
}

void isoTimestampUsesGregorianEpochAndExplicitZone() {
  struct Case { const char* date; uint64_t expected; };
  const Case cases[] = {
      {"1970-01-01T00:00:00Z", 0},
      {"2000-01-01T00:00:00Z", 946684800ULL},
      {"2024-01-01T00:00:00Z", 1704067200ULL},
      {"2024-01-01T01:00:00+01:00", 1704067200ULL},
      {"2023-12-31 23:00:00-01:00", 1704067200ULL},
      {"2024-01-01T14:00:00+14:00", 1704067200ULL},
      {"2023-12-31T10:00:00-14:00", 1704067200ULL}
  };
  for (const auto& item : cases) {
    uint64_t actual = 99;
    CHECK(gateway::parsePlcTimestamp(item.date, DateFormat::IsoYearFirst,
                                    gateway::kUnknownUtcOffsetMinutes, actual));
    CHECK(actual == item.expected);
  }
}

void naiveTimestampRequiresConfirmedTimezoneAndChosenFormat() {
  uint64_t actual = 99;
  CHECK(gateway::parsePlcTimestamp("2024-01-01 01:00:00", DateFormat::IsoYearFirst, 60, actual));
  CHECK(actual == 1704067200ULL);
  CHECK(gateway::parsePlcTimestamp("01/01/2024 01:00:00", DateFormat::DayFirstSlash, 60, actual));
  CHECK(actual == 1704067200ULL);
  CHECK(gateway::parsePlcTimestamp("2023-12-31T23:00:00", DateFormat::IsoYearFirst, -60, actual));
  CHECK(actual == 1704067200ULL);
  CHECK(gateway::parsePlcTimestamp("2024-01-01 14:00:00", DateFormat::IsoYearFirst, 840, actual));
  CHECK(actual == 1704067200ULL);
  CHECK(gateway::parsePlcTimestamp("2023-12-31 10:00:00", DateFormat::IsoYearFirst, -840, actual));
  CHECK(actual == 1704067200ULL);
}

void timestampAcceptsLeapDaysAndUpperYearBoundary() {
  uint64_t actual = 0;
  CHECK(gateway::parsePlcTimestamp("2024-02-29T00:00:00Z", DateFormat::IsoYearFirst, 0, actual));
  CHECK(actual == 1709164800ULL);  // January 1 plus 59 days.
  CHECK(gateway::parsePlcTimestamp("2000-02-29T00:00:00Z", DateFormat::IsoYearFirst, 0, actual));
  CHECK(actual == 951782400ULL);   // Leap century year, divisible by 400.
  CHECK(gateway::parsePlcTimestamp("2099-12-31T23:59:59Z", DateFormat::IsoYearFirst, 0, actual));
  CHECK(actual == 4102444799ULL);
}

void invalidTimestampPreservesOutput() {
  const char* invalid[] = {
      "", "2024-01-01", "2024-1-01T00:00:00Z", "2024-01-1T00:00:00Z",
      "2024-01-01T0:00:00Z", "2024-01-01T00:0:00Z", "2024-01-01T00:00:0Z",
      "1969-12-31T23:59:59Z", "2100-01-01T00:00:00Z", "2023-02-29T00:00:00Z",
      "2024-02-30T00:00:00Z", "2024-04-31T00:00:00Z", "2024-00-01T00:00:00Z",
      "2024-13-01T00:00:00Z", "2024-01-00T00:00:00Z", "2024-01-32T00:00:00Z",
      "2024-01-01T24:00:00Z", "2024-01-01T00:60:00Z", "2024-01-01T00:00:60Z",
      "2024-01-01T00:00:00.1Z", "2024-01-01T00:00:00Zx", "2024-01-01T00:00:00Z ",
      "2024-01-01T00:00:00+14:01", "2024-01-01T00:00:00-14:01",
      "2024-01-01T00:00:00+15:00", "2024-01-01T00:00:00+01:60",
      "2024-01-01T00:00:00+0100", "1970-01-01T00:00:00+00:01",
      "01/01/2024 00:00:00", "2024-01-01X00:00:00Z"
  };
  for (const char* date : invalid) {
    uint64_t actual = 987654321ULL;
    CHECK(!gateway::parsePlcTimestamp(date, DateFormat::IsoYearFirst, 0, actual));
    CHECK(actual == 987654321ULL);
  }
  for (int offset : {gateway::kUnknownUtcOffsetMinutes, -841, 841}) {
    uint64_t actual = 42;
    CHECK(!gateway::parsePlcTimestamp("2024-01-01 00:00:00", DateFormat::IsoYearFirst, offset, actual));
    CHECK(actual == 42);
  }
  for (DateFormat format : {DateFormat::Unconfigured, static_cast<DateFormat>(99)}) {
    uint64_t actual = 42;
    CHECK(!gateway::parsePlcTimestamp("2024-01-01T00:00:00Z", format, 0, actual));
    CHECK(actual == 42);
  }
  uint64_t actual = 42;
  CHECK(!gateway::parsePlcTimestamp(nullptr, DateFormat::IsoYearFirst, 0, actual));
  CHECK(actual == 42);
  for (const char* date : {"2024-01-01 00:00:00", "01/01/2024 00:00:00Z",
                           "31/04/2024 00:00:00", "29/02/2023 00:00:00",
                           "1/01/2024 00:00:00", "01/01/2024T00:00:00"}) {
    CHECK(!gateway::parsePlcTimestamp(date, DateFormat::DayFirstSlash, 0, actual));
    CHECK(actual == 42);
  }
  CHECK(!gateway::parsePlcTimestamp("01/01/2024 00:00:00", DateFormat::DayFirstSlash,
                                   gateway::kUnknownUtcOffsetMinutes, actual));
  CHECK(actual == 42);
}

void telemetryHasExactlyAdvancedThesisSchema() {
  Sample sample = expectedSample();
  sample.timestamp = 1704067200ULL;
  char output[512] = {};
  const size_t bytes = gateway::encodeTelemetry(sample, "ESP32_TEST", true, output, sizeof(output));
  CHECK(bytes > 0 && bytes < sizeof(output));
  if (bytes == 0 || bytes >= sizeof(output)) return;
  CHECK(output[bytes] == '\0');
  CHECK(std::strlen(output) == bytes);
  JsonDocument doc;
  const auto error = deserializeJson(doc, output, bytes);
  CHECK(!error);
  if (error) return;
  CHECK(doc.is<JsonObject>());
  CHECK(doc.size() == 8);
  CHECK(std::strcmp(doc["EspID"] | "", "ESP32_TEST") == 0);
  CHECK(doc["TimeStamp"].is<uint64_t>());
  CHECK(doc["TimeStamp"].as<uint64_t>() == 1704067200ULL);
  CHECK(doc["Spool_Fbk"].as<float>() == 1.25F);
  CHECK(doc["Spool_CMD"].as<float>() == -2.5F);
  CHECK(doc["Axe_Position_Fbk"].as<float>() == 3.0F);
  CHECK(doc["Axe_Position_Reference"].as<float>() == 4.0F);
  CHECK(doc["Hyd_Pressure"].as<float>() == 5.0F);
  CHECK(doc["PLC_Status"].is<bool>());
  CHECK(doc["PLC_Status"].as<bool>());
}

void telemetryEscapesDeviceIdAndPreservesUint64Timestamp() {
  Sample sample;
  sample.timestamp = std::numeric_limits<uint64_t>::max();
  const char* device = "ESP\"slash\\line\n";
  char output[512] = {};
  const size_t bytes = gateway::encodeTelemetry(sample, device, false, output, sizeof(output));
  CHECK(bytes > 0 && bytes < sizeof(output));
  if (bytes == 0 || bytes >= sizeof(output)) return;
  JsonDocument doc;
  const auto error = deserializeJson(doc, output, bytes);
  CHECK(!error);
  if (error) return;
  CHECK(std::strcmp(doc["EspID"] | "", device) == 0);
  CHECK(doc["TimeStamp"].is<uint64_t>());
  CHECK(doc["TimeStamp"].as<uint64_t>() == 18446744073709551615ULL);
  CHECK(doc["PLC_Status"].is<bool>());
  CHECK(!doc["PLC_Status"].as<bool>());
}

void telemetryFailsForInvalidInputsOrInsufficientCapacity() {
  Sample sample = expectedSample();
  char large[512] = {};
  const size_t needed = gateway::encodeTelemetry(sample, "ESP", true, large, sizeof(large));
  CHECK(needed > 0 && needed < sizeof(large));
  if (needed == 0 || needed >= sizeof(large)) return;
  std::vector<char> exact(needed + 2, '#');
  CHECK(gateway::encodeTelemetry(sample, "ESP", true, exact.data(), needed + 1) == needed);
  CHECK(exact[needed] == '\0');
  CHECK(exact[needed + 1] == '#');
  for (size_t capacity : {size_t(0), size_t(1), needed}) {
    std::vector<char> bounded(needed + 2, '#');
    CHECK(gateway::encodeTelemetry(sample, "ESP", true, bounded.data(), capacity) == 0);
    CHECK(bounded[capacity] == '#');
  }
  CHECK(gateway::encodeTelemetry(sample, "ESP", true, nullptr, 512) == 0);
  CHECK(gateway::encodeTelemetry(sample, nullptr, true, large, sizeof(large)) == 0);
  CHECK(gateway::encodeTelemetry(sample, "", true, large, sizeof(large)) == 0);
  const std::string tooLong(2048, 'x');
  CHECK(gateway::encodeTelemetry(sample, tooLong.c_str(), true, large, sizeof(large)) == 0);
  float Sample::* metrics[] = {&Sample::spoolFeedback, &Sample::spoolCommand,
      &Sample::axisPositionFeedback, &Sample::axisPositionReference, &Sample::hydraulicPressure};
  for (auto metric : metrics) {
    for (float value : {std::numeric_limits<float>::infinity(),
                        -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
      Sample invalid = sample;
      invalid.*metric = value;
      CHECK(gateway::encodeTelemetry(invalid, "ESP", true, large, sizeof(large)) == 0);
    }
  }
}

void relayTopicsRequireExactMatch() {
  CHECK(gateway::relayIndex("esp32/relay1") == 0);
  CHECK(gateway::relayIndex("esp32/relay2") == 1);
  CHECK(gateway::relayIndex("esp32/relay3") == 2);
  CHECK(gateway::relayIndex("esp32/relay4") == 3);
  for (const char* topic : {"", "esp32/relay0", "esp32/relay5", "esp32/relay10",
                            "esp32/relay1/extra", "esp32/relay1x", "xesp32/relay1",
                            "/esp32/relay1", "ESP32/relay1", "esp32/relay"}) {
    CHECK(gateway::relayIndex(topic) == -1);
  }
  CHECK(gateway::relayIndex(nullptr) == -1);
}

bool command(const std::string& json, bool& enabled) {
  return gateway::decodeRelayCommand(reinterpret_cast<const uint8_t*>(json.data()), json.size(), enabled);
}

void relayCommandsAcceptOnlySupportedStatusRepresentations() {
  bool enabled = false;
  CHECK(command("{\"status\":1}", enabled)); CHECK(enabled);
  CHECK(command("{\"status\":0}", enabled)); CHECK(!enabled);
  CHECK(command("{\"status\":\"1\"}", enabled)); CHECK(enabled);
  CHECK(command("{\"status\":\"0\"}", enabled)); CHECK(!enabled);
  CHECK(command(" \t{ \"status\" : 1 }\r\n", enabled)); CHECK(enabled);
}

void rejectedCommandsPreservePriorRelayValue() {
  const char* invalid[] = {
      "", " ", "{", "{\"status\":", "{\"status\":1", "{status:1}",
      "{}", "{\"other\":1}", "{\"status\":true}", "{\"status\":false}",
      "{\"status\":null}", "{\"status\":1.0}", "{\"status\":0.0}",
      "{\"status\":1e0}", "{\"status\":2}", "{\"status\":-1}",
      "{\"status\":4294967297}", "{\"status\":\"2\"}", "{\"status\":\"01\"}",
      "{\"status\":\" 1\"}", "{\"status\":\"true\"}", "{\"status\":\"\"}",
      "{\"status\":[]}", "{\"status\":{}}", "[{\"status\":1}]",
      "null", "true", "1", "\"1\"", "{\"status\":1}garbage",
      "{\"status\":1}{\"status\":0}", "{\"status\":1,\"extra\":0}",
      "{\"status\":1,\"status\":0}"
  };
  for (const char* json : invalid) {
    for (bool prior : {false, true}) {
      bool enabled = prior;
      CHECK(!command(json, enabled));
      CHECK(enabled == prior);
    }
  }
}

void relayCommandsRespectExplicitByteLengthAndBounds() {
  CHECK(gateway::kMaxCommandBytes == 128);
  const uint8_t raw[] = {'{', '"', 's', 't', 'a', 't', 'u', 's', '"', ':', '1', '}'};
  bool enabled = false;
  CHECK(gateway::decodeRelayCommand(raw, sizeof(raw), enabled)); CHECK(enabled);
  enabled = true;
  CHECK(!gateway::decodeRelayCommand(raw, sizeof(raw) - 1, enabled)); CHECK(enabled);
  const char outside[] = "{\"status\":0}OUTSIDE_THE_PAYLOAD";
  CHECK(gateway::decodeRelayCommand(reinterpret_cast<const uint8_t*>(outside), 12, enabled));
  CHECK(!enabled);
  enabled = true;
  CHECK(!gateway::decodeRelayCommand(nullptr, 12, enabled)); CHECK(enabled);
  CHECK(!gateway::decodeRelayCommand(nullptr, 0, enabled)); CHECK(enabled);
  CHECK(!gateway::decodeRelayCommand(raw, 0, enabled)); CHECK(enabled);
  std::string maximum = "{\"status\":0}";
  maximum.append(128 - maximum.size(), ' ');
  CHECK(command(maximum, enabled)); CHECK(!enabled);
  maximum.push_back(' ');
  enabled = true;
  CHECK(!command(maximum, enabled)); CHECK(enabled);
  const std::string nested = "{\"status\":1,\"extra\":" + std::string(30, '[') + "0" + std::string(30, ']') + "}";
  CHECK(nested.size() < 128);
  enabled = false;
  CHECK(!command(nested, enabled)); CHECK(!enabled);
}

void staleHeartbeatDoesNotReviveAfterFullMillisCycle() {
  gateway::HeartbeatMonitor heartbeat(12000);
  heartbeat.observe(7, 100);
  CHECK(heartbeat.healthy(100));
  heartbeat.observe(7, 12100);
  CHECK(!heartbeat.healthy(12100));
  // Valid reads continue, but a full cycle of the 32-bit timer cannot revive an
  // expired heartbeat without a counter change.
  heartbeat.observe(7, 100);
  CHECK(!heartbeat.healthy(100));
  heartbeat.observe(8, 101);
  CHECK(heartbeat.healthy(101));
}

void run(const char* name, void (*test)()) {
  ++tests;
  const int before = failures;
  test();
  std::cout << (failures == before ? "PASS " : "FAIL ") << name << '\n';
}
}  // namespace

int main() {
  run("S7 advanced frame offsets", s7FrameUsesAdvancedOffsetsAndDeclaredStringLength);
  run("explicit raw date encoding", rawStringAtOffsetTwoRequiresExplicitEncoding);
  run("frame invalid inputs preserve all fields", frameRejectionsPreserveAllFields);
  run("S7 string header bounds", s7StringValidatesHeaderAndEmbeddedNul);
  run("raw date terminator bounds", rawStringMustTerminateBeforeMetrics);
  run("every metric rejects nonfinite frames", everyNonfiniteMetricRejectsWholeFrame);
  run("five metric equality excludes metadata", equalityTracksFiveMetricsAndIgnoresMetadata);
  run("publication success and PLC health", publicationBaselineIncludesHealthButOnlySuccessfulPublications);
  run("interval attempt boundaries", intervalGateUsesAttemptTimesAndExactBoundary);
  run("interval millis rollover", intervalGateHandlesMillisRollover);
  run("heartbeat observation and frozen counter", heartbeatRequiresObservationAndActualCounterChange);
  run("heartbeat millis rollover", heartbeatHandlesMillisRollover);
  run("stale heartbeat across full millis cycle", staleHeartbeatDoesNotReviveAfterFullMillisCycle);
  run("inactive GPIO initialization", activeLowRelayStartsInactiveUnderEsp32RegistrationRules);
  run("ISO explicit timezone and known epochs", isoTimestampUsesGregorianEpochAndExplicitZone);
  run("naive timestamps require configured timezone", naiveTimestampRequiresConfirmedTimezoneAndChosenFormat);
  run("leap dates and year boundary", timestampAcceptsLeapDaysAndUpperYearBoundary);
  run("invalid timestamps preserve output", invalidTimestampPreservesOutput);
  run("exact telemetry schema", telemetryHasExactlyAdvancedThesisSchema);
  run("escaped ID and uint64 timestamp", telemetryEscapesDeviceIdAndPreservesUint64Timestamp);
  run("telemetry validation and full buffer", telemetryFailsForInvalidInputsOrInsufficientCapacity);
  run("exact relay topic matching", relayTopicsRequireExactMatch);
  run("accepted command representations", relayCommandsAcceptOnlySupportedStatusRepresentations);
  run("strict command grammar and output preservation", rejectedCommandsPreservePriorRelayValue);
  run("bounded command payload", relayCommandsRespectExplicitByteLengthAndBounds);
  std::cout << tests << " tests, " << assertions << " assertions, " << failures << " failures\n";
  return failures == 0 ? 0 : 1;
}
