#include "GatewayProtocol.h"

#include <ArduinoJson.h>
#include <cmath>
#include <cstring>

namespace gateway {

std::size_t encodeTelemetry(const Sample& sample, const char* deviceId,
                          bool plcHealthy, char* output, std::size_t capacity) {
    if (deviceId == nullptr || deviceId[0] == '\0' || output == nullptr || capacity == 0 ||
        !std::isfinite(sample.spoolFeedback) || !std::isfinite(sample.spoolCommand) ||
        !std::isfinite(sample.axisPositionFeedback) ||
        !std::isfinite(sample.axisPositionReference) ||
        !std::isfinite(sample.hydraulicPressure)) {
        return 0;
    }

    JsonDocument document;
    document["EspID"] = deviceId;
    document["TimeStamp"] = sample.timestamp;
    document["Spool_Fbk"] = sample.spoolFeedback;
    document["Spool_CMD"] = sample.spoolCommand;
    document["Axe_Position_Fbk"] = sample.axisPositionFeedback;
    document["Axe_Position_Reference"] = sample.axisPositionReference;
    document["Hyd_Pressure"] = sample.hydraulicPressure;
    document["PLC_Status"] = plcHealthy;
    if (document.overflowed()) {
        return 0;
    }

    const std::size_t required = measureJson(document);
    if (required >= capacity) {
        return 0;
    }
    const std::size_t written = serializeJson(document, output, capacity);
    if (written != required) {
        output[0] = '\0';
        return 0;
    }
    output[written] = '\0';
    return written;
}
int relayIndex(const char* topic) {
    if (topic == nullptr) {
        return -1;
    }
    for (std::size_t i = 0; i < kRelayCount; ++i) {
        if (std::strcmp(topic, kRelayTopics[i]) == 0) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

namespace {

bool isWhitespace(std::uint8_t value) {
    return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}

class CommandReader {
public:
    CommandReader(const std::uint8_t* data, std::size_t length) : data_(data), length_(length) {}

    void skipWhitespace() {
        while (position_ < length_ && isWhitespace(data_[position_])) {
            ++position_;
        }
    }

    bool consume(char value) {
        if (position_ >= length_ || data_[position_] != static_cast<std::uint8_t>(value)) {
            return false;
        }
        ++position_;
        return true;
    }

    bool consumeKey() {
        for (const char value : "\"status\"") {
            if (value != '\0' && !consume(value)) {
                return false;
            }
        }
        return true;
    }

    bool readStatus(bool& enabled) {
        const bool quoted = consume('"');
        bool value;
        if (consume('0')) {
            value = false;
        } else if (consume('1')) {
            value = true;
        } else {
            return false;
        }
        if (quoted && !consume('"')) {
            return false;
        }
        enabled = value;
        return true;
    }

    bool atEnd() const { return position_ == length_; }

private:
    const std::uint8_t* data_;
    std::size_t length_;
    std::size_t position_ = 0;
};

}  // namespace

bool decodeRelayCommand(const std::uint8_t* payload, std::size_t length, bool& enabled) {
    if (payload == nullptr || length == 0 || length > kMaxCommandBytes) {
        return false;
    }
    // GPIO commands intentionally contain only the literal "status" field. This
    // restricted grammar rejects duplicate keys, trailing data, and nested objects
    // without allocating memory in the MQTT message handler.
    CommandReader reader(payload, length);
    reader.skipWhitespace();
    if (!reader.consume('{')) {
        return false;
    }
    reader.skipWhitespace();
    if (!reader.consumeKey()) {
        return false;
    }
    reader.skipWhitespace();
    if (!reader.consume(':')) {
        return false;
    }
    reader.skipWhitespace();
    bool value = false;
    if (!reader.readStatus(value)) {
        return false;
    }
    reader.skipWhitespace();
    if (!reader.consume('}')) {
        return false;
    }
    reader.skipWhitespace();
    if (!reader.atEnd()) {
        return false;
    }
    enabled = value;
    return true;
}

}  // namespace gateway
