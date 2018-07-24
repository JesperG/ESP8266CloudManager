/*
 *  This sketch demonstrates how to scan WiFi networks.
 *  The API is almost the same as with the WiFi Shield library,
 *  the most obvious difference being the different file you need to include:
 */
#include <ESP8266WiFi.h>
//#include <ESP8266WiFiMulti.h>
// #include <ESP8266HTTPClient.h>

// WiFiManager
//#include <DNSServer.h>
//#include <ESP8266WebServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager

// #include <ArduinoJson.h>

#include <PubSubClient.h>
WiFiClient espClient;
PubSubClient mqttLocalClient(espClient);


#include "ESP8266CloudManager.h"

//ESP8266WiFiMulti WiFiMulti;

#include <TaskScheduler.h>

// DoubleResetDetector
#include <DoubleResetDetector.h>
// Number of seconds after reset during which a
// subseqent reset will be considered a double reset.
#define DRD_TIMEOUT 1
// RTC Memory Address for the DoubleResetDetector to use
#define DRD_ADDRESS 0
DoubleResetDetector drd(DRD_TIMEOUT, DRD_ADDRESS);


Scheduler runner;

// https://github.com/arkhipenko/TaskScheduler/blob/master/src/TaskSchedulerDeclarations.h
// INLINE Task(unsigned long aInterval=0, long aIterations=0, TaskCallback aCallback=NULL, Scheduler* aScheduler=NULL, bool aEnable=false, TaskOnEnable aOnEnable=NULL, TaskOnDisable aOnDisable=NULL);
Task tDrdLoop(50 * TASK_MILLISECOND, 2*DRD_TIMEOUT*(1000/50), &tDrdLoopCallback);
Task tCheckForUpdates(24 * TASK_HOUR, TASK_FOREVER, tCheckForUpdatesCallback);
Task tMqttLocalReconnect(5 * TASK_SECOND, TASK_FOREVER, &tMqttLocalReconnectCallback);
Task tMqttLocalClientLoop(10 * TASK_MILLISECOND, TASK_FOREVER, &tMqttLocalClientLoopCallback);
Task tHeartBeat(1 * TASK_SECOND, TASK_FOREVER, &tHeartBeatCallback);

void tHeartBeatCallback()
{
  unsigned long elapsed = millis();
  char tx_data[8+1];
  sprintf(tx_data, "%08lX", elapsed);

  String msg = getMAC() + " " + String(tx_data) + " " + "Hi! My name is: " + configName;

  Serial.println(msg);
  if (mqttLocalClient.connected())
  {
    // Publish on topic "log/<MAC>".
    // Can be subscribed directly or using topic "log/#"
    mqttLocalClient.publish(String("log/" + getMAC()).c_str(), msg.c_str());

  }
}

// ###### MQTT ######
void onMqttLocalPayloadReceived(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();


  if ((char)payload[0] == 'R') {
    Serial.println("Restarting now");
    ESP.restart();
  }

  if ((char)payload[0] == 'U') {
    Serial.println("Scheduling a check for updates");
    tCheckForUpdates.forceNextIteration();
  }

  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == '1') {
    digitalWrite(BUILTIN_LED, LOW);   // Turn the LED on (Note that LOW is the voltage level
    // but actually the LED is on; this is because
    // it is acive low on the ESP-01)
  } else {
    digitalWrite(BUILTIN_LED, HIGH);  // Turn the LED off by making the voltage HIGH
  }

}

void tMqttLocalReconnectCallback() {
  // Loop until we're reconnected
  if (!mqttLocalClient.connected())
  {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    String clientName = String("ESP8266Client-") + getMAC();
    if (mqttLocalClient.connect(clientName.c_str())) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      mqttLocalClient.publish(String("log/" + getMAC()).c_str(), String(clientName + " connected").c_str());
      // ... and resubscribe
      mqttLocalClient.subscribe("dev/all");
      mqttLocalClient.subscribe(String("dev/" + getMAC()).c_str());
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttLocalClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      //delay(5000);
    }
  }
}

void tMqttLocalClientLoopCallback()
{
  mqttLocalClient.loop();
}


void tCheckForUpdatesCallback()
{
  LoadJsonConfig();
}

void tDrdLoopCallback()
{
  drd.loop();
}



void setup() {

  Serial.begin(115200);
  Serial.println("Starting");
  //WiFi.mode(WIFI_STA);
  //WiFiMulti.addAP(ssid, password);

  Serial.print("Current ESP.getSketchMD5(): ");
  Serial.println(ESP.getSketchMD5());

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  //reset saved settings
  if (drd.detectDoubleReset())
  {
    Serial.println("DoubleReset detected");
    // wifiManager.resetSettings();
  }
  //set custom ip for portal
  //wifiManager.setAPStaticIPConfig(IPAddress(10,0,1,1), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
  //fetches ssid and pass from eeprom and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  wifiManager.autoConnect("AutoConnectAP", "qwertyuiop");
  //or use this for auto generated name ESP + ChipID
  //wifiManager.autoConnect();

  // Can be "autoconfigured" using http://192.168.4.1/wifisave?s=<SSID>&p=<PWD>

  LoadJsonConfig();

  // Schedule the tasks which needs to run.
  runner.init();
  runner.addTask(tDrdLoop);
  tDrdLoop.enable();

  if (!configSet)
  {

  }
  else
  {
    pinMode(BUILTIN_LED, OUTPUT);
    mqttLocalClient.setServer(configMQTTServer.c_str(), 1883);
    mqttLocalClient.setCallback(onMqttLocalPayloadReceived);

    runner.addTask(tMqttLocalReconnect);
    tMqttLocalReconnect.enable();
    runner.addTask(tMqttLocalClientLoop);
    tMqttLocalClientLoop.enable();

    runner.addTask(tHeartBeat);
    tHeartBeat.enable();
  }

  runner.addTask(tCheckForUpdates);
  tCheckForUpdates.enable();



}



void loop()
{
    runner.execute();
}
