#include "TimeManager.h"
#include <Arduino.h>

void TimeManager::configureNTP(const char* ntpServer, long gmtOffset_sec, int daylightOffset_sec) {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

bool TimeManager::getLocalTime(struct tm* localTime) {
    if (!getLocalTime(localTime)) {
        Serial.println("Failed to get local time");
    }
    return true;
}

time_t TimeManager::getCurrentTime() {
    return time(NULL);
}