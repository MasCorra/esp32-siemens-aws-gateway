#include "PLCManager.h"

bool PLCManager::IsOn;
float PLCManager::Pressure;
int PLCManager::Components[10];
bool PLCManager::IsOnCOMP;
float PLCManager::PressureCOMP;
int PLCManager::ComponentsCOMP[10];
bool PLCManager::DataChanged;

S7Client PLCManager::Client;

bool PLCManager::connectToPLC(const IPAddress& plcIP, int rack, int slot) {
    pingPLC(plcIP);
    int result = Client.ConnectTo(plcIP, rack, slot);
    if (result == 0) {
        Serial.print("Connected! PDU Length = ");
        Serial.println(Client.GetPDULength());
        return true;
    } else {
        Serial.println("Connection error");
        return false;
    }
}

bool PLCManager::readDataFromPLC(int DBNum, int offset, int Size, void* Target) {
    int result = Client.ReadArea(S7AreaDB, DBNum, offset, Size, Target);
    if (result == 0) {
        // Pass Data from Buffer to constants
        IsOn = S7.BitAt(0, 0);
        Pressure = S7.FloatAt(2);
        for (int i = 0; i < 10; i++) {
            Components[i] = S7.IntegerAt(6 + i * 2);
        }

        // Compare current data with previous data
        if (IsOn != IsOnCOMP || Pressure != PressureCOMP) {
            DataChanged = true;
            IsOnCOMP = IsOn;
            PressureCOMP = Pressure;
            for (int i = 0; i < 10; i++) {
                if (Components[i] != ComponentsCOMP[i]) {
                    DataChanged = true;
                    ComponentsCOMP[i] = Components[i];
                }
            }
        } else {
            DataChanged = false;
        }

        return true;
    } else {
        checkError(result);
        return false;
    }
}



void PLCManager::checkError(int ErrNo) {
    Serial.print("Error No. 0x");
    Serial.println(ErrNo, HEX);
    if (ErrNo & 0x00FF) {
        Serial.println("SEVERE ERROR, disconnecting");
        Client.Disconnect();
    }
}

void PLCManager::pingPLC(const IPAddress& plcIP) {
    bool ret = Ping.ping(plcIP, 4);
    if (ret) {
        Serial.println("Ping ... successful!");
        float avg_time_ms = Ping.averageTime();
        Serial.print("Average ping time: ");
        Serial.print(avg_time_ms);
        Serial.println(" ms");
    } else {
        Serial.println("Ping failed!");
    }
}