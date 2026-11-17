// Main Code
#include "WiFiManager.h"
#include "AWSManager.h"
#include "PLCManager.h"
#include "TimeManager.h"
#include "DataManager.h"
#include "secrets.h"

// Relay Pins
#define Relay1 15
#define Relay2 2
#define Relay3 4
#define Relay4 22

#define AWS_IOT_PUBLISH_TOPIC "esp32/counter"
#define AWS_IOT_SUBSCRIBE_TOPIC1 "esp32/relay1"
#define AWS_IOT_SUBSCRIBE_TOPIC2 "esp32/relay2"
#define AWS_IOT_SUBSCRIBE_TOPIC3 "esp32/relay3"
#define AWS_IOT_SUBSCRIBE_TOPIC4 "esp32/relay4"
#define DO_IT_SMALL  // Small buffer
#define BLYNK_TEMPLATE_ID "TMPL?????-u"
#define BLYNK_TEMPLATE_NAME "ESP32DHT11"
#include "Blynk.h"

BlynkTimer timer;

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;

char jsonBuffer[512];
IPAddress PLCip(192, 168, 1, 217);
int c = 0;
int DBNum = 2;
byte Buffer[1024];
int Size, Result;
void *Target;
bool IsOn;
float Pressure;
int Components[10];
bool IsOnCOMP;
float PressureCOMP;
int ComponentsCOMP[10];
bool DataChanged = false;
unsigned long Elapsed;
struct tm ora_locale;
time_t now;

void setup() {

    Serial.begin(115200);
    while (!Serial) {
        ;                   //wait for serial to port to connect. Needed for Leonardo only
    }

    Utility::markTime();

    WiFiManager::connectToWiFi(WIFI_SSID, WIFI_PASSWORD);
    AWSManager::connectToAWS(THINGNAME, AWS_CERT_CA, AWS_CERT_CRT, AWS_CERT_PRIVATE, AWS_IOT_ENDPOINT);
    PLCManager::connectToPLC(PLCip, 0, 1);
    TimeManager::configureNTP(ntpServer, gmtOffset_sec, daylightOffset_sec);

    Utility::showTime();
    Serial.println("Esp32 configured correctly!");
    

    pinMode(Relay1, OUTPUT);
    pinMode(Relay2, OUTPUT);
    pinMode(Relay3, OUTPUT);
    pinMode(Relay4, OUTPUT);
}

void loop() {

    #ifdef DO_IT_SMALL
        Size = 26;
        Target = NULL; //Uses the internal Buffer (PDU.DATA[])
    #else
        Size = 1024;
        Target = &Buffer; //Uses a larger buffer
    #endif

    //Connection
    while (!PLCManager::Client.Connected) {
        if (!PLCManager::connectToPLC(PLCip, 0, 1))
            delay(500);
    }

    Serial.print("Reading ");
    Serial.print(Size);
    Serial.print(" bytes from DB");
    Serial.println(DBNum);

    // Get the current tick
    Utility::markTime();
    //Read data
    PLCManager::readDataFromPLC(DBNum, 0, Size, Target);
    // Process and publish data
    DataManager::processData(IsOn, Pressure, Components, DataChanged, IsOnCOMP, PressureCOMP, ComponentsCOMP);
    DataManager::stampData(IsOn, Pressure, Components);
    TimeManager::getLocalTime(&ora_locale);

    if (DataChanged==true){
    DataManager::publishToAWS(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);
    }
    Utility::showTime();
    delay(500);

}
