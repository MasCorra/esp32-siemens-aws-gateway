#ifndef DATAMANAGER_H
#define DATAMANAGER_H

#include <ArduinoJson.h>

class DataManager {
public:
    static void publishToAWS(const char* topic, const char* message);
    static void processData(bool& isOn, float& pressure, int components[], bool& dataChanged, bool& isOnComp, float& pressureComp, int componentsComp[]);
    static void stampData(bool isOn, float pressure, int components[]);
};
#endif 