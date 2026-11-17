#ifndef TIMEMANAGER_H
#define TIMEMANAGER_H

#include <time.h>

class TimeManager {
public:
    static void configureNTP(const char* ntpServer, long gmtOffset_sec, int daylightOffset_sec);
    static bool getLocalTime(struct tm* localTime);
    static time_t getCurrentTime();
};

#endif // TIMEMANAGER_H