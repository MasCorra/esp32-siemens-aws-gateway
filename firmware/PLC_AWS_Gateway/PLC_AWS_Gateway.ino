#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <Settimino.h>
#include <cstring>
#include <cstdio>
#include <ctime>

#include "GatewayConfig.h"
#include "GatewayCore.h"
#include "GatewayProtocol.h"

#if __has_include("secrets.h")
#include "secrets.h"
#else
// A fresh checkout can compile; placeholder values never initiate a connection.
#include "secrets.h.example"
#endif
#ifndef GATEWAY_DEVICE_ID
#define GATEWAY_DEVICE_ID ""
#endif

namespace config = gateway_config;

namespace {

WiFiClientSecure tls;
PubSubClient mqtt(tls);
S7Client plc;
gateway::PublicationTracker publication;
gateway::HeartbeatMonitor heartbeat(config::kHeartbeatTimeoutMs);
gateway::IntervalGate wifiRetry(config::kWifiRetryMs);
gateway::IntervalGate plcRetry(config::kPlcRetryMs);
gateway::IntervalGate mqttRetry(config::kMqttRetryMs);
gateway::IntervalGate sampleInterval(config::kSampleIntervalMs);
gateway::IntervalGate publishRetry(config::kPublishRetryMs);

gateway::Sample latestSample;
bool hasSample = false;
bool latestReadValid = false;
bool wifiWasConnected = false;
bool configured = false;
char deviceId[65] = {};
std::uint8_t plcFrame[gateway::kFrameSize] = {};
char telemetry[config::kTelemetryBufferBytes] = {};

static_assert(sizeof(config::kRelayPins) / sizeof(config::kRelayPins[0]) == gateway::kRelayCount,
              "Relay topic and GPIO counts must match");
static_assert(config::kTelemetryBufferBytes + sizeof("esp32/counter") + 7 <=
                  config::kMqttBufferBytes,
              "MQTT buffer must include topic and protocol overhead");

bool hasValue(const char* value) {
    return value != nullptr && value[0] != '\0' &&
           std::strstr(value, "YOUR_") == nullptr && std::strstr(value, "PASTE_") == nullptr;
}

bool credentialsConfigured() {
    return hasValue(WIFI_SSID) && hasValue(WIFI_PASSWORD) && hasValue(THINGNAME) &&
           hasValue(AWS_IOT_ENDPOINT) && hasValue(AWS_CERT_CA) &&
           hasValue(AWS_CERT_CRT) && hasValue(AWS_CERT_PRIVATE);
}

bool projectConfigured() {
    const bool encodingKnown = config::kDateEncoding == gateway::DateEncoding::S7StringHeaderAt0 ||
                               config::kDateEncoding == gateway::DateEncoding::NullTerminatedAt2;
    const bool formatKnown = config::kDateFormat == gateway::DateFormat::IsoYearFirst ||
                             config::kDateFormat == gateway::DateFormat::DayFirstSlash;
    const bool offsetKnown = config::kPlcUtcOffsetMinutes >= -840 && config::kPlcUtcOffsetMinutes <= 840;
    // ISO strings with Z or an explicit offset do not require a default time zone.
    const bool timezoneUsable = offsetKnown ||
        (config::kDateFormat == gateway::DateFormat::IsoYearFirst &&
         config::kPlcUtcOffsetMinutes == gateway::kUnknownUtcOffsetMinutes);
    return config::kDbNumber != 0 && encodingKnown && formatKnown && timezoneUsable;
}

bool initializeDeviceId() {
    int written;
    if (GATEWAY_DEVICE_ID[0] != '\0') {
        written = std::snprintf(deviceId, sizeof(deviceId), "%s", GATEWAY_DEVICE_ID);
    } else {
        const auto id = static_cast<unsigned long long>(ESP.getEfuseMac() & 0xFFFFFFFFFFFFULL);
        written = std::snprintf(deviceId, sizeof(deviceId), "%012llX", id);
    }
    return written > 0 && static_cast<std::size_t>(written) < sizeof(deviceId);
}

bool clockReady() {
    return std::time(nullptr) >= config::kEarliestValidEpoch;
}

void handleMessage(char* topic, byte* payload, unsigned int length) {
    if (!config::kEnableRelays) {
        return;
    }
    const int index = gateway::relayIndex(topic);
    bool enabled = false;
    if (index < 0 || !gateway::decodeRelayCommand(payload, length, enabled)) {
        Serial.println("Ignored invalid relay command");
        return;
    }
    digitalWrite(config::kRelayPins[index], enabled ? LOW : HIGH);
}

void serviceWifi() {
    if (WiFi.status() == WL_CONNECTED) {
        if (!wifiWasConnected) {
            wifiWasConnected = true;
            configTzTime(config::kTimezone, config::kNtpServer);
            Serial.println("WiFi connected; clock synchronization requested");
        }
        return;
    }

    if (wifiWasConnected) {
        // Preserve the last sample's timestamp for a possible status-only notification,
        // without presenting old data as a new PLC acquisition.
        tls.stop();
        plc.Disconnect();
        latestReadValid = false;
        heartbeat.invalidate();
        wifiWasConnected = false;
        Serial.println("WiFi disconnected");
    }
    if (wifiRetry.ready(millis())) {
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        wifiRetry.mark(millis());
        Serial.println("WiFi connection attempt");
    }
}

void serviceMqtt() {
    if (mqtt.connected()) {
        mqtt.loop();
        return;
    }
    // TLS certificate validation requires a plausible clock. NTP runs asynchronously,
    // allowing other services to continue during synchronization.
    if (!clockReady() || !mqttRetry.ready(millis())) {
        return;
    }

    const bool connected = mqtt.connect(THINGNAME);
    mqttRetry.mark(millis());
    if (!connected) {
        tls.stop();
        Serial.printf("MQTT connection failed: %d\n", mqtt.state());
        return;
    }
    if (config::kEnableRelays) {
        for (const auto topic : gateway::kRelayTopics) {
            if (!mqtt.subscribe(topic)) {
                mqtt.disconnect();
                tls.stop();
                Serial.println("MQTT subscription failed; will retry");
                return;
            }
        }
    }
    // Send current data after each new MQTT session, even if it has not changed.
    publication = gateway::PublicationTracker{};
    Serial.println("MQTT connected");
}

void servicePlc() {
    if (!plc.Connected) {
        if (!plcRetry.ready(millis())) {
            return;
        }
        const IPAddress address(config::kPlcAddress);
        const int result = plc.ConnectTo(address, config::kRack, config::kSlot);
        plcRetry.mark(millis());
        if (result != 0) {
            latestReadValid = false;
            heartbeat.invalidate();
            Serial.printf("PLC connection failed: 0x%04X\n", result);
            return;
        }
        Serial.println("PLC connected");
    }
    if (!sampleInterval.ready(millis())) {
        return;
    }

    const int result = plc.ReadArea(S7AreaDB, config::kDbNumber, 0,
                                    gateway::kFrameSize, plcFrame);
    sampleInterval.mark(millis());
    gateway::Sample decoded;
    const bool valid = result == 0 &&
        gateway::decodeFrame(plcFrame, sizeof(plcFrame), config::kDateEncoding, decoded) &&
        gateway::parsePlcTimestamp(decoded.date, config::kDateFormat,
                                   config::kPlcUtcOffsetMinutes, decoded.timestamp);
    if (!valid) {
        latestReadValid = false;
        heartbeat.invalidate();
        if (result != 0) {
            plc.Disconnect();
            plcRetry.mark(millis());
        }
        Serial.printf("PLC sample rejected: 0x%04X\n", result);
        return;
    }

    latestSample = decoded;
    hasSample = true;
    latestReadValid = true;
    // Check the heartbeat even when no process measurements have changed.
    heartbeat.observe(decoded.messageCounter, millis());
}

void servicePublish() {
    const bool plcHealthy = latestReadValid && plc.Connected && heartbeat.healthy(millis());
    if (!mqtt.connected() || !hasSample || !publication.pending(latestSample, plcHealthy) ||
        !publishRetry.ready(millis())) {
        return;
    }

    const std::size_t length = gateway::encodeTelemetry(
        latestSample, deviceId, plcHealthy, telemetry, sizeof(telemetry));
    publishRetry.mark(millis());
    if (length == 0) {
        Serial.println("Telemetry encoding failed; sample remains pending");
        return;
    }

    const bool sent = mqtt.publish(gateway::kPublishTopic,
        reinterpret_cast<const std::uint8_t*>(telemetry), static_cast<unsigned int>(length));
    // PubSubClient publishes with QoS 0: true means a local write, not cloud acknowledgment.
    publication.recordResult(latestSample, plcHealthy, sent);
    if (sent) {
        Serial.printf("Telemetry written: %u bytes\n", static_cast<unsigned int>(length));
    } else {
        Serial.println("MQTT write failed; sample remains pending");
    }
}

}  // namespace

void setup() {
    Serial.begin(115200);
    if (config::kEnableRelays) {
        for (const auto pin : config::kRelayPins) {
            gateway::initializeActiveLowOutput(pin,
                [](std::uint8_t gpio, bool output) { pinMode(gpio, output ? OUTPUT : INPUT); },
                [](std::uint8_t gpio, bool high) { digitalWrite(gpio, high ? HIGH : LOW); });
        }
    }

    configured = credentialsConfigured() && projectConfigured();
    if (!configured) {
        Serial.println("Complete secrets.h and confirm PLC DB/date settings in GatewayConfig.h");
        return;
    }
    if (!initializeDeviceId()) {
        configured = false;
        Serial.println("Device ID must contain 1 to 64 bytes");
        return;
    }
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);
    plc.RecvTimeout = config::kPlcReceiveTimeoutMs;
    tls.setCACert(AWS_CERT_CA);
    tls.setCertificate(AWS_CERT_CRT);
    tls.setPrivateKey(AWS_CERT_PRIVATE);
    tls.setConnectionTimeout(config::kTcpTimeoutMs);
    tls.setHandshakeTimeout(config::kTlsHandshakeTimeoutSeconds);
    mqtt.setServer(AWS_IOT_ENDPOINT, config::kMqttPort);
    mqtt.setCallback(handleMessage);
    mqtt.setKeepAlive(config::kMqttKeepAliveSeconds);
    mqtt.setSocketTimeout(config::kMqttSocketTimeoutSeconds);
    if (!mqtt.setBufferSize(config::kMqttBufferBytes)) {
        configured = false;
        Serial.println("MQTT buffer allocation failed");
    }
}

void loop() {
    if (configured) {
        serviceWifi();
        if (WiFi.status() == WL_CONNECTED) {
            serviceMqtt();
            servicePlc();
            // Service MQTT again after the synchronous PLC call returns.
            if (mqtt.connected()) {
                mqtt.loop();
            }
            servicePublish();
        }
    }
    delay(1);  // Yield to ESP32 networking and RTOS tasks without a fixed 5 s pause.
}
