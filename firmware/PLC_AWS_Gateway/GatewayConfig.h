#pragma once

#include <cstddef>
#include <cstdint>
#include "GatewayCore.h"

// Unconfirmed PLC settings prevent startup. Set them only after checking
// the PLC project. Compile-only values are not valid deployment settings.
#ifndef GATEWAY_PLC_DB_NUMBER
#define GATEWAY_PLC_DB_NUMBER 0
#endif
#ifndef GATEWAY_PLC_DATE_ENCODING
#define GATEWAY_PLC_DATE_ENCODING 0
#endif
#ifndef GATEWAY_PLC_DATE_FORMAT
#define GATEWAY_PLC_DATE_FORMAT 0
#endif
#ifndef GATEWAY_PLC_UTC_OFFSET_MINUTES
#define GATEWAY_PLC_UTC_OFFSET_MINUTES 32767
#endif

namespace gateway_config {

// Bench defaults for address, rack, and slot; verify them against the target PLC.
constexpr std::uint8_t kPlcAddress[] = {192, 168, 1, 217};
constexpr std::uint16_t kDbNumber = GATEWAY_PLC_DB_NUMBER;
constexpr auto kDateEncoding = static_cast<gateway::DateEncoding>(GATEWAY_PLC_DATE_ENCODING);
constexpr auto kDateFormat = static_cast<gateway::DateFormat>(GATEWAY_PLC_DATE_FORMAT);
constexpr int kPlcUtcOffsetMinutes = GATEWAY_PLC_UTC_OFFSET_MINUTES;
constexpr std::uint16_t kRack = 0;
constexpr std::uint16_t kSlot = 1;
// Initial timing defaults: the expected 5 s heartbeat does not define the sampling interval.
constexpr std::uint32_t kSampleIntervalMs = 250;
constexpr std::uint32_t kHeartbeatTimeoutMs = 12000;
constexpr std::uint32_t kWifiRetryMs = 10000;
constexpr std::uint32_t kPlcRetryMs = 5000;
constexpr std::uint32_t kMqttRetryMs = 5000;
constexpr std::uint32_t kPublishRetryMs = 1000;
constexpr std::uint16_t kPlcReceiveTimeoutMs = 500;
constexpr std::uint32_t kTcpTimeoutMs = 2000;
constexpr std::uint16_t kTlsHandshakeTimeoutSeconds = 3;
constexpr std::uint16_t kMqttSocketTimeoutSeconds = 2;
constexpr std::uint16_t kMqttKeepAliveSeconds = 30;
constexpr std::uint16_t kMqttPort = 8883;
constexpr std::uint16_t kMqttBufferBytes = 1280;
constexpr std::size_t kTelemetryBufferBytes = 1024;

constexpr const char* kNtpServer = "pool.ntp.org";
// NTP supports certificate validation; published timestamps come from the PLC.
constexpr const char* kTimezone = "UTC0";
constexpr std::int64_t kEarliestValidEpoch = 1704067200;  // 2024-01-01 UTC

// Default GPIO mapping with active-low commands. Check bootstrapping pins and
// external relay circuitry for the target ESP32 board before connecting them.
// Relay commands are optional and disabled until their use has been validated.
constexpr bool kEnableRelays = false;
constexpr std::uint8_t kRelayPins[] = {15, 2, 4, 22};

static_assert(GATEWAY_PLC_DB_NUMBER >= 0 && GATEWAY_PLC_DB_NUMBER <= 65535,
              "DB number must fit a Siemens DB identifier");
static_assert(GATEWAY_PLC_DATE_ENCODING >= 0 && GATEWAY_PLC_DATE_ENCODING <= 2,
              "Unsupported PLC date encoding");
static_assert(GATEWAY_PLC_DATE_FORMAT >= 0 && GATEWAY_PLC_DATE_FORMAT <= 2,
              "Unsupported PLC date format");

}  // namespace gateway_config
