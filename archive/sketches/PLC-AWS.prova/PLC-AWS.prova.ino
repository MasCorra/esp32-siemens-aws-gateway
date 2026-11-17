//#define ARDUINO_LAN    
//#define ESP8266_FAMILY  
#define ESP32_WIFI
//#define M5STACK_WIFI
//#define M5STACK_LAN
#include "Platform.h"             //PLC connection libraries
#include "Settimino.h"            //PLC connection libraries
#include "secrets.h"              //Contains all the credentials 
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ESP32Ping.h>            //Ping Libraries
//Uncomment next line to perform small and fast access
#define DO_IT_SMALL
//Blynk defines
#define BLYNK_TEMPLATE_ID           "TMPL?????-u"
#define BLYNK_TEMPLATE_NAME         "ESP32DHT11"
// Relay Pins
#define Relay1 15
#define Relay2 2
#define Relay3 4
#define Relay4 22
//Included just to use it's timer function(Blynk cloud platfrom is not used in this code)
#include <Blynk.h>
// Topics of MQTT
#define AWS_IOT_PUBLISH_TOPIC   "esp32/counter"
#define AWS_IOT_SUBSCRIBE_TOPIC1 "esp32/relay1"
#define AWS_IOT_SUBSCRIBE_TOPIC2 "esp32/relay2"
#define AWS_IOT_SUBSCRIBE_TOPIC3 "esp32/relay3"
#define AWS_IOT_SUBSCRIBE_TOPIC4 "esp32/relay4"

BlynkTimer timer;
WiFiClientSecure net = WiFiClientSecure();
PubSubClient client(net);          //PubSub communication
S7Client Client;                   //S7Client will create a EthernetClient as TCP Client

//Enter a MAC address and IP for your controller below.
byte mac[]={ 0x90, 0xA2, 0xDA, 0x0F, 0x08, 0xE1 };
// The IP adress will be dependent on your local network
IPAddress Local(192, 168, 59, 247);          //Local Address
IPAddress PLCip(192, 168, 59, 217);          //PLC Address
IPAddress Gateway(192, 168, 59, 117);
IPAddress Subnet(255, 255, 255, 0);
//Counter Initialisation
int c = 0; 
int DBNum = 1; //This DB must be present in your PLC
byte Buffer[1024];

unsigned long Elapsed; //To calc the execution time


//------------------------------------------------------------------
// SETUP: Connect Esp32 to WiFi - Connect Esp32 to AWS 
//------------------------------------------------------------------
void setup() 
{
  //Open serial communications and wait for port to open;
  Serial.begin(115200);
  while (!Serial) {
    ; //wait for serial to port to connect. Needed for Leonardo only
  }
  connectWiFi();
  connectAWS();

  pinMode(Relay1, OUTPUT);
  pinMode(Relay2, OUTPUT);
  pinMode(Relay3, OUTPUT);
  pinMode(Relay4, OUTPUT);

  timer.setInterval(5000L, myTimerEvent); //Calls this "timer event function" every 5 seconds

}


//------------------------------------------------------------------
//MAIN LOOP
//------------------------------------------------------------------
void loop() {

  timer.run(); // Initiates BlynkTimer
  client.loop();
  
  int Size, Result;
  void *Target;
#ifdef DO_IT_SMALL
  Size=10;
  Target = NULL;     //Uses the internal Buffer (PDU.DATA[])
#else
  Size=1024;
  Target = &Buffer;  //Uses a larger buffer
#endif

  //Connection
  while (!Client.Connected) {
    if (!ConnectToPLC())
      delay(500);
  }
  
  Serial.print ("Reading ");Serial.print(Size);Serial.print(" bytes from DB");Serial.println(DBNum);
  //Get the current tick
  MarkTime();
  Result=Client.ReadArea(S7AreaDB, //We are requesting DB access
                        DBNum,     //DB Number
                        0,         //Start from byte N.0
                        Size,      //We need "Size" bytes
                        Target);   //Put them into our target (Buffer or PDU)
  if (Result==0) {
    ShowTime();
    Dump(Target, Size);
  }
  else
    CheckError(Result);

  delay(500);
}


// Timer Callback function
void myTimerEvent()
{

  StaticJsonDocument<200> doc;
  doc["message"] = "Hello from ESP32";
  doc["Counter"] = c;
  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer); // print to client
  client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);
  Serial.println("Message Published");
  c++;
Serial.println(c);
Serial.println(AWS_IOT_PUBLISH_TOPIC);

}


//------------------------------------------------------------------
//Connects to the WiFi
//------------------------------------------------------------------
void connectWiFi () {
#ifdef S7WIFI
//--------------------------------------------- ESP8266 Initialization    
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    while (WiFi.status() != WL_CONNECTED) 
    {
        delay(500);
        Serial.print(".");
        //Serial.print(WiFi.status());
    }
    Serial.println("");
    Serial.println("WiFi connected");  
    Serial.print("Local IP address : ");
    Serial.println(WiFi.localIP());
    // Print subnet mask
    Serial.print("Subnet mask: ");
    Serial.println(WiFi.subnetMask());
    // Print gateway IP
    Serial.print("Gateway IP: ");
    Serial.println(WiFi.gatewayIP());
#else
//--------------------------------Wired Ethernet Shield Initialization    
    // Start the Ethernet Library
    EthernetInit(mac, Local);
    // Setup Time, someone said me to leave 2000 because some 
    // rubbish compatible boards are a bit deaf.
    delay(2000); 
    Serial.println("");
    Serial.println("Cable connected");  
    Serial.print("Local IP address : ");
    Serial.println(Ethernet.localIP());
#endif   
}

//------------------------------------------------------------------
//Connects to AWS
//------------------------------------------------------------------
void connectAWS()
{
  // Configure WiFiClientSecure to use the AWS IoT device credentials
  net.setCACert(AWS_CERT_CA);
  net.setCertificate(AWS_CERT_CRT);
  net.setPrivateKey(AWS_CERT_PRIVATE);

  // Connect to the MQTT broker on the AWS endpoint we defined earlier
  client.setServer(AWS_IOT_ENDPOINT, 8883);

  // Create a message handler
  client.setCallback(messageHandler);

  Serial.println("Connecting to AWS IOT ");

  while (!client.connect(THINGNAME)) {
    Serial.print(".");
    Serial.println(client.state());
    delay(100);
  }

  if (!client.connected()) {
    Serial.println("AWS IoT Timeout!");
    return;
  }

  // Subscribe to a topic
  client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC1);
  client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC2);
  client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC3);
  client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC4);

  Serial.println("AWS IoT Connected!");  
}

//------------------------------------------------------------------
//Connects to the PLC
//------------------------------------------------------------------
bool ConnectToPLC() {

  // Call the PingPLC function
  PingPLC(PLCip);
  
  Serial.println("PLC connection");
  int result = Client.ConnectTo(PLCip, 0, 1); // Replace 0 and 2 with your Rack and Slot numbers
  Serial.print("Connecting to ");Serial.println(PLCip);
  
  if (result==0)
    {
      Serial.print("Connected ! PDU Length = ");Serial.println(Client.GetPDULength());
    }
  else
      Serial.println ("Connection error");
  return result==0;
}
//------------------------------------------------------------------
//PingPLC function
//------------------------------------------------------------------
void PingPLC(const IPAddress& PLCip) {
  // Call the PingPLC function
  bool ret = Ping.ping(PLCip, 4);
  if (ret) {
    Serial.println("Ping successful!");
    float avg_time_ms = Ping.averageTime();
    Serial.print("Average ping time: ");
    Serial.print(avg_time_ms);
    Serial.println(" ms");
  } else {
    Serial.println("Ping failed!");
  }
}


void messageHandler(char* topic, byte* payload, unsigned int length)
{
  Serial.print("incoming: ");
  Serial.println(topic);

  if ( strstr(topic, "esp32/relay1") )
  {
    StaticJsonDocument<200> doc;
    deserializeJson(doc, payload);
    String Relay_data = doc["status"];
    int r = Relay_data.toInt();
    digitalWrite(Relay1, !r);
    Serial.print("Relay1 - "); Serial.println(Relay_data);
  }

  if ( strstr(topic, "esp32/relay2") )
  {
    StaticJsonDocument<200> doc;
    deserializeJson(doc, payload);
    String Relay_data = doc["status"];
    int r = Relay_data.toInt();
    digitalWrite(Relay2, !r);
    Serial.print("Relay2 - "); Serial.println(Relay_data);
  }

  if ( strstr(topic, "esp32/relay3") )
  {
    StaticJsonDocument<200> doc;
    deserializeJson(doc, payload);
    String Relay_data = doc["status"];
    int r = Relay_data.toInt();
    digitalWrite(Relay3, !r);
    Serial.print("Relay3 - "); Serial.println(Relay_data);
  }

  if ( strstr(topic, "esp32/relay4") )
  {
    StaticJsonDocument<200> doc;
    deserializeJson(doc, payload);
    String Relay_data = doc["status"];
    int r = Relay_data.toInt();
    digitalWrite(Relay4, !r);
    Serial.print("Relay4 - "); Serial.println(Relay_data);
  }


}

//------------------------------------------------------------------
//Dumps a buffer (a very rough routine)
//------------------------------------------------------------------
void Dump (void *Buffer, int Length) {
  int i, cnt=0;
  pbyte buf;

  if(Buffer!=NULL)
    buf = pbyte(Buffer);
  else
    buf = pbyte(&PDU.DATA[0]);

  Serial.print("[ Dumping ");Serial.print(Length);
  Serial.println(" bytes ]=========================");
  for (i=0;i<Length;i++) {
    cnt++;
    if (buf[i]<0x10)
      Serial.print("0");
    Serial.print(buf[i], HEX);
    Serial.print(" ");
    if(cnt==16)
    {
      cnt=0;
      Serial.println();
    }
  }
  Serial.println("=======================");
}
//------------------------------------------------------------------
//Prints the Error number
//------------------------------------------------------------------
void CheckError (int ErrNo) {
  Serial.print("Error No. 0x");
  Serial.println(ErrNo, HEX);

  //Check if it's a Server Error => we need to disconnect
  if (ErrNo & 0x00FF)
  {
    Serial.println("SEVERE ERROR, disconnecting");
    Client.Disconnect();
  }
}
//------------------------------------------------------------------
//Profiling routines
//------------------------------------------------------------------
void MarkTime() {
  Elapsed=millis();
}
//------------------------------------------------------------------
void ShowTime() {
  //Calcs the time
  Elapsed=millis()-Elapsed;
  Serial.print("Job time (ms) :");
  Serial.println(Elapsed);
}

