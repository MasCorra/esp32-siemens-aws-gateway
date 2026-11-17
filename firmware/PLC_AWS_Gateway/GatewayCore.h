#pragma once

#include <cstddef>
#include <cstdint>

namespace gateway {

constexpr std::size_t kFrameSize = 312;
constexpr std::size_t kDateCapacity = 255;
constexpr int kUnknownUtcOffsetMinutes = 32767;

enum class DateEncoding { Unconfigured, S7StringHeaderAt0, NullTerminatedAt2 };
enum class DateFormat { Unconfigured, IsoYearFirst, DayFirstSlash };

struct Sample {
    char date[kDateCapacity] = {};
    float spoolFeedback = 0;
    float spoolCommand = 0;
    float axisPositionFeedback = 0;
    float axisPositionReference = 0;
    float hydraulicPressure = 0;
    std::int16_t counterPlc = 0;
    std::int16_t messageCounter = 0;
    std::uint64_t timestamp = 0;
};

bool decodeFrame(const std::uint8_t* data, std::size_t size, DateEncoding encoding, Sample& output);
bool parsePlcTimestamp(const char* text, DateFormat format, int utcOffsetMinutes,
                       std::uint64_t& output);
// Compare measurements only: dates and heartbeat counters do not determine duplicates.
bool sameSample(const Sample& left, const Sample& right);

template <typename SetOutputMode, typename SetLevel>
void initializeActiveLowOutput(std::uint8_t pin, SetOutputMode setOutputMode, SetLevel setLevel) {
    // On ESP32, digitalWrite requires pinMode to register the GPIO. Configure it as
    // an input first, preload the inactive level, then enable it as an output.
    setOutputMode(pin, false);
    setLevel(pin, true);
    setOutputMode(pin, true);
}

class PublicationTracker {
public:
    bool pending(const Sample& sample, bool plcHealthy) const;
    void recordResult(const Sample& sample, bool plcHealthy, bool published);

private:
    Sample published_{};
    bool hasPublished_ = false;
    bool publishedHealthy_ = false;
};

class HeartbeatMonitor {
public:
    explicit HeartbeatMonitor(std::uint32_t timeoutMs);
    void observe(std::int16_t counter, std::uint32_t now);
    bool healthy(std::uint32_t now) const;
    void invalidate();

private:
    std::uint32_t timeoutMs_;
    std::uint32_t lastChange_ = 0;
    std::int16_t counter_ = 0;
    bool observed_ = false;
    bool expired_ = false;
};

class IntervalGate {
public:
    explicit IntervalGate(std::uint32_t intervalMs);
    bool ready(std::uint32_t now) const;
    void mark(std::uint32_t now);

private:
    std::uint32_t intervalMs_;
    std::uint32_t last_ = 0;
    bool started_ = false;
};

}  // namespace gateway
