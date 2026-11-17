// PLCManager.h
#ifndef PLCMANAGER_H
#define PLCMANAGER_H

#include "Platform.h" //PLC connection libraries
#include "Settimino.h" //PLC connection libraries
#include <IPAddress.h>
#include "ESP32Ping.h"

class PLCManager {
public:
    static bool connectToPLC(const IPAddress& plcIP, int rack, int slot);
    static bool readDataFromPLC(int DBNum, int offset, int Size, void* Target);
    static void checkError(int ErrNo);
    static S7Client Client;

    // Declare the variables used in the readDataFromPLC() function
    static bool IsOn;
    static float Pressure;
    static int Components[10];
    static bool IsOnCOMP;
    static float PressureCOMP;
    static int ComponentsCOMP[10];
    static bool DataChanged;

private:
    static void pingPLC(const IPAddress& plcIP);
};
#endif 