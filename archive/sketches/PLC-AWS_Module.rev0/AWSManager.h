#ifndef AWSMANAGER_H
#define AWSMANAGER_H

#include <WiFiClientSecure.h>
#include <PubSubClient.h>

class AWSManager {
public:
    static bool connectToAWS(const char* THINGHNAME, const char* awsCertCA, const char* awsCertCRT, const char* awsCertPrivateKey, const char* awsIoTEndpoint);
    static void publishToAWS(const char* topic, const char* message);
    static void subscribeToAWS(const char* topic, void (*callback)(char*, byte*, unsigned int));

private:
    static WiFiClientSecure net;
    static PubSubClient client;
    static void messageHandler(char* topic, byte* payload, unsigned int length);
};
#endif 