#include "GatewayCore.h"

#include <cmath>
#include <cstring>
#include <limits>

namespace gateway {
namespace {

float readReal(const std::uint8_t* data) {
    // Assemble the PLC's big-endian bytes before interpreting them as a float.
    const std::uint32_t bits =
        (std::uint32_t(data[0]) << 24U) | (std::uint32_t(data[1]) << 16U) |
        (std::uint32_t(data[2]) << 8U) | std::uint32_t(data[3]);
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

std::int16_t readInt(const std::uint8_t* data) {
    const std::uint16_t raw = (std::uint16_t(data[0]) << 8U) | data[1];
    const std::int32_t value = raw <= 32767U ? raw : std::int32_t(raw) - 65536;
    return static_cast<std::int16_t>(value);
}

bool leapYear(int year) {
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

int readDigits(const char* text, std::size_t count) {
    int value = 0;
    for (std::size_t i = 0; i < count; ++i) {
        if (text[i] < '0' || text[i] > '9') {
            return -1;
        }
        value = value * 10 + (text[i] - '0');
    }
    return value;
}

}  // namespace

bool decodeFrame(const std::uint8_t* data, std::size_t size,
                 DateEncoding encoding, Sample& output) {
    if (data == nullptr || size < kFrameSize) {
        return false;
    }
    static_assert(sizeof(float) == sizeof(std::uint32_t), "PLC REAL requires 32-bit float");
    static_assert(std::numeric_limits<float>::is_iec559, "PLC REAL requires IEEE-754");

    std::size_t dateLength = 0;
    switch (encoding) {
        case DateEncoding::S7StringHeaderAt0:
            if (data[0] == 0 || data[0] > 254 || data[1] == 0 || data[1] > data[0]) {
                return false;
            }
            dateLength = data[1];
            for (std::size_t i = 0; i < dateLength; ++i) {
                if (data[2 + i] == 0) {
                    return false;
                }
            }
            break;
        case DateEncoding::NullTerminatedAt2:
            while (dateLength < 254 && data[2 + dateLength] != 0) {
                ++dateLength;
            }
            if (dateLength == 0 || dateLength == 254) {
                return false;
            }
            break;
        default:
            return false;
    }

    Sample decoded;
    std::memcpy(decoded.date, data + 2, dateLength);
    decoded.date[dateLength] = '\0';
    decoded.spoolFeedback = readReal(data + 256);
    decoded.spoolCommand = readReal(data + 260);
    decoded.axisPositionFeedback = readReal(data + 264);
    decoded.axisPositionReference = readReal(data + 268);
    decoded.hydraulicPressure = readReal(data + 272);
    if (!std::isfinite(decoded.spoolFeedback) || !std::isfinite(decoded.spoolCommand) ||
        !std::isfinite(decoded.axisPositionFeedback) ||
        !std::isfinite(decoded.axisPositionReference) ||
        !std::isfinite(decoded.hydraulicPressure)) {
        return false;
    }
    decoded.counterPlc = readInt(data + 308);
    decoded.messageCounter = readInt(data + 310);
    // Update the output only after validating the entire data block.
    output = decoded;
    return true;
}

bool parsePlcTimestamp(const char* text, DateFormat format, int utcOffsetMinutes,
                       std::uint64_t& output) {
    if (text == nullptr) {
        return false;
    }
    // The longest accepted format is ISO with a signed HH:MM time zone offset.
    std::size_t length = 0;
    while (length <= 25 && text[length] != '\0') {
        ++length;
    }
    if (length < 19 || length > 25) {
        return false;
    }

    int year, month, day;
    int offsetMinutes = utcOffsetMinutes;
    switch (format) {
        case DateFormat::IsoYearFirst:
            if (text[4] != '-' || text[7] != '-' ||
                (text[10] != ' ' && text[10] != 'T')) {
                return false;
            }
            year = readDigits(text, 4);
            month = readDigits(text + 5, 2);
            day = readDigits(text + 8, 2);
            if (length == 20 && text[19] == 'Z') {
                offsetMinutes = 0;
            } else if (length == 25 && (text[19] == '+' || text[19] == '-') &&
                       text[22] == ':') {
                const int zoneHours = readDigits(text + 20, 2);
                const int zoneMinutes = readDigits(text + 23, 2);
                if (zoneHours < 0 || zoneHours > 14 || zoneMinutes < 0 ||
                    zoneMinutes > 59 || (zoneHours == 14 && zoneMinutes != 0)) {
                    return false;
                }
                offsetMinutes = (zoneHours * 60 + zoneMinutes) * (text[19] == '-' ? -1 : 1);
            } else if (length != 19) {
                return false;
            }
            break;
        case DateFormat::DayFirstSlash:
            if (length != 19 || text[2] != '/' || text[5] != '/' || text[10] != ' ') {
                return false;
            }
            year = readDigits(text + 6, 4);
            month = readDigits(text + 3, 2);
            day = readDigits(text, 2);
            break;
        default:
            return false;
    }

    if (text[13] != ':' || text[16] != ':' || offsetMinutes < -840 || offsetMinutes > 840) {
        return false;
    }
    const int hour = readDigits(text + 11, 2);
    const int minute = readDigits(text + 14, 2);
    const int second = readDigits(text + 17, 2);
    if (year < 1970 || year > 2099 || month < 1 || month > 12 || day < 1 ||
        hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59) {
        return false;
    }
    const int monthLengths[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const int daysInMonth = monthLengths[month - 1] + (month == 2 && leapYear(year) ? 1 : 0);
    if (day > daysInMonth) {
        return false;
    }

    std::int64_t days = 0;
    for (int previousYear = 1970; previousYear < year; ++previousYear) {
        days += leapYear(previousYear) ? 366 : 365;
    }
    for (int previousMonth = 1; previousMonth < month; ++previousMonth) {
        days += monthLengths[previousMonth - 1];
        if (previousMonth == 2 && leapYear(year)) {
            ++days;
        }
    }
    days += day - 1;
    const std::int64_t timestamp = days * 86400 + hour * 3600 + minute * 60 + second -
                                   std::int64_t(offsetMinutes) * 60;
    if (timestamp < 0) {
        return false;
    }
    output = static_cast<std::uint64_t>(timestamp);
    return true;
}

bool sameSample(const Sample& left, const Sample& right) {
    return left.spoolFeedback == right.spoolFeedback &&
           left.spoolCommand == right.spoolCommand &&
           left.axisPositionFeedback == right.axisPositionFeedback &&
           left.axisPositionReference == right.axisPositionReference &&
           left.hydraulicPressure == right.hydraulicPressure;
}

bool PublicationTracker::pending(const Sample& sample, bool plcHealthy) const {
    return !hasPublished_ || plcHealthy != publishedHealthy_ || !sameSample(sample, published_);
}

void PublicationTracker::recordResult(const Sample& sample, bool plcHealthy, bool published) {
    if (published) {
        published_ = sample;
        publishedHealthy_ = plcHealthy;
        hasPublished_ = true;
    }
}

HeartbeatMonitor::HeartbeatMonitor(std::uint32_t timeoutMs) : timeoutMs_(timeoutMs) {}

void HeartbeatMonitor::observe(std::int16_t counter, std::uint32_t now) {
    if (!observed_ || counter != counter_) {
        lastChange_ = now;
        counter_ = counter;
        observed_ = true;
        expired_ = false;
    } else if (std::uint32_t(now - lastChange_) >= timeoutMs_) {
        // After expiration, only a changed counter or an explicit reset can restore health,
        // even after a full cycle of the 32-bit millisecond timer.
        expired_ = true;
    }
}

bool HeartbeatMonitor::healthy(std::uint32_t now) const {
    return observed_ && !expired_ && std::uint32_t(now - lastChange_) < timeoutMs_;
}

void HeartbeatMonitor::invalidate() {
    observed_ = false;
    expired_ = false;
}

IntervalGate::IntervalGate(std::uint32_t intervalMs) : intervalMs_(intervalMs) {}

bool IntervalGate::ready(std::uint32_t now) const {
    return !started_ || std::uint32_t(now - last_) >= intervalMs_;
}

void IntervalGate::mark(std::uint32_t now) {
    last_ = now;
    started_ = true;
}

}  // namespace gateway
