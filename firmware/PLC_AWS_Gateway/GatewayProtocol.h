#pragma once

#include "GatewayCore.h"

namespace gateway {

constexpr std::size_t kMaxCommandBytes = 128;
constexpr const char* kPublishTopic = "esp32/counter";
constexpr const char* kRelayTopics[] = {
    "esp32/relay1", "esp32/relay2", "esp32/relay3", "esp32/relay4"
};
constexpr std::size_t kRelayCount = 4;

std::size_t encodeTelemetry(const Sample& sample, const char* deviceId,
                          bool plcHealthy, char* output, std::size_t capacity);
int relayIndex(const char* topic);
bool decodeRelayCommand(const std::uint8_t* payload, std::size_t length, bool& enabled);

}  // namespace gateway
