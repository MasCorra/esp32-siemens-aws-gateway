#include "WiFiManager.h"

bool WiFiManager::connectToWiFi(const char* ssid, const char* password) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("WiFi connected");
    return true;
}

IPAddress WiFiManager::getLocalIP() {
    return WiFi.localIP();
}

IPAddress WiFiManager::getSubnetMask() {
    return WiFi.subnetMask();
}

IPAddress WiFiManager::getGatewayIP() {
    return WiFi.gatewayIP();
}
