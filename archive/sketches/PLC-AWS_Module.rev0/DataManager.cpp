#include "DataManager.h"
#include "AWSManager.h"

void DataManager::publishToAWS(const char* topic, const char* message) {
    AWSManager::publishToAWS(topic, message);
}

void DataManager::processData(bool& isOn, float& pressure, int components[], bool& DataChanged, bool& isOnComp, float& pressureComp, int componentsComp[]) {
    // Compare current data with previous data
    if (isOn != isOnComp || pressure != pressureComp) {
        DataChanged = true;
        isOnComp = isOn;
        pressureComp = pressure;
        for (int i = 0; i < 10; i++) {
            if (components[i] != componentsComp[i]) {
                DataChanged = true;
                componentsComp[i] = components[i];
            }
        }
    } else {
        DataChanged = false;
    }
}

void DataManager::stampData(bool isOn, float pressure, int components[]) {
    Serial.print("Machine State = ");
    if (isOn) {
        Serial.println("On");
    } else {
        Serial.println("Off");
    }
    Serial.print("Reading Pressure = ");
    Serial.println(pressure);
    for (int i = 0; i < 10; i++) {
        Serial.print("Components ");
        Serial.print(i + 1);
        Serial.print(" value is: ");
        Serial.println(components[i]);
    }
}
