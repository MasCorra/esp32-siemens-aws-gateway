#include "AWSManager.h"

WiFiClientSecure AWSManager::net = WiFiClientSecure();
PubSubClient AWSManager::client = PubSubClient(AWSManager::net);

bool AWSManager::connectToAWS(const char* THINGNAME, const char* awsCertCA, const char* awsCertCRT, const char* awsCertPrivateKey, const char* awsIoTEndpoint) {
    net.setCACert(awsCertCA);
    net.setCertificate(awsCertCRT);
    net.setPrivateKey(awsCertPrivateKey);
    client.setServer(awsIoTEndpoint, 8883);
    client.setCallback(messageHandler);

    while (!client.connect(THINGNAME)) {
        Serial.print(".");
        Serial.println(client.state());
        delay(100);
    }

    if (!client.connected()) {
        Serial.println("AWS IoT Timeout!");
        return false;
    }

    Serial.println("AWS IoT Connected!");
    return true;
}

void AWSManager::publishToAWS(const char* topic, const char* message) {
    client.publish(topic, message);
}

void AWSManager::subscribeToAWS(const char* topic, void (*callback)(char*, byte*, unsigned int)) {
    client.subscribe(topic);
    client.setCallback(callback);
}

void AWSManager::messageHandler(char* topic, byte* payload, unsigned int length) {
    Serial.print("incoming: ");
    Serial.println(topic);
    // Handle MQTT subscriptions
}
