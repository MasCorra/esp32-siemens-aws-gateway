// WiFiManager.h
#ifndef WIFIMANAGER_H
#define WIFIMANAGER_H

#include <WiFi.h>

class WiFiManager {
public:
    static bool connectToWiFi(const char* ssid, const char* password);
    static IPAddress getLocalIP();
    static IPAddress getSubnetMask();
    static IPAddress getGatewayIP();
};
#endif 