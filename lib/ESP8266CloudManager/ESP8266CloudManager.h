
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <ESP8266httpUpdate.h>

#include "ESP8266CloudManagerPrivate.h" // const char* fwUrlBase = ""; // Moved to unversioned file


bool configSet = false; // Has the config been set?
String configName = ""; // "Slim shady"
String configSketchMD5 = ""; // "7799e9d...."
String configMQTTServer = "";
int configAutoUpdate = -1; // 1
// Use

bool UpdateByRootJson(String json)
{
// arduinojson.org/assistant
  const size_t bufferSize = JSON_OBJECT_SIZE(4) + 256; // 256

  DynamicJsonBuffer jsonBuffer(bufferSize);
  JsonObject& root = jsonBuffer.parseObject(json);

  const char* name = root["name"]; // "Slim shady"
  const char* SketchMD5 = root["SketchMD5"]; // "7799e9db..."
  int AutoUpdate = root["AutoUpdate"]; // 1
  const char* MQTTServer = root["MQTTServer"];

  configName = String(name);
  configSketchMD5 = String(SketchMD5);
  configAutoUpdate = AutoUpdate;
  configMQTTServer = String(MQTTServer);
  configSet = true;
  return true;
}

// http OTA
// https://www.bakke.online/index.php/2017/06/02/self-updating-ota-firmware-for-esp8266/

String getMAC()
{
  uint8_t mac[6];
  char result[14];
  WiFi.macAddress(mac);
  snprintf( result, sizeof( result ), "%02x%02x%02x%02x%02x%02x", mac[ 0 ], mac[ 1 ], mac[ 2 ], mac[ 3 ], mac[ 4 ], mac[ 5 ] );
  return String( result );
}

void LoadJsonConfig()
{
  Serial.println("Loading json config from web.");
  String jsonConfigUrl = String(fwUrlBase);
  //String mac = getMAC();
  jsonConfigUrl.concat("device-");
  jsonConfigUrl.concat( getMAC() );
  jsonConfigUrl.concat( ".json" );

  Serial.print("Downloading : ");
  Serial.println(jsonConfigUrl);
  HTTPClient httpClient;
  httpClient.begin( jsonConfigUrl );
  int httpCode = httpClient.GET();
  if( httpCode == 200 )
  {
    String jsonConfig = httpClient.getString();
    httpClient.end();
    //DynamicJsonBuffer jsonBuffer;
    // You can use a String as your JSON input.
    // WARNING: the content of the String will be duplicated in the JsonBuffer.
    /*
    JsonObject& root = jsonBuffer.parseObject(jsonConfig);
    // jsonRoot = jsonBuffer.parseObject(jsonConfig);
    String targetSketchMD5 = root["SketchMD5"];

    bool doAutoUpdate = root["AutoUpdate"].as<int>() == 1;
    */
    String currentSketchMD5 = ESP.getSketchMD5();

    if (!UpdateByRootJson(jsonConfig))
    {
      Serial.println("Error when reading json config file. Exiting LoadJsonConfig().");
      return;
    }
    else
    {
      Serial.print("Target SketchMD5: ");
      Serial.println(configSketchMD5);
      Serial.print("Current SketchMD5: ");
      Serial.println(currentSketchMD5);
      if (currentSketchMD5 == configSketchMD5)
      {
        Serial.println("Firmware is OK.");
      }
      else
      {
        Serial.println("Time for new firmware!");
        String fwImageURL = fwUrlBase;
        fwImageURL.concat( "firmware-" );
        fwImageURL.concat(configSketchMD5);
        fwImageURL.concat( ".bin" );
        Serial.print( "Firmware bin URL: " );
        Serial.println( fwImageURL );
        if (configAutoUpdate)
        {
          Serial.println("--- Initiating OTA update ---");
          t_httpUpdate_return ret = ESPhttpUpdate.update( fwImageURL );

          switch(ret) {
            case HTTP_UPDATE_FAILED:
              Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
              Serial.println();
              break;

            case HTTP_UPDATE_NO_UPDATES:
              Serial.println("HTTP_UPDATE_NO_UPDATES");
              break;

            default:
              Serial.print("ESPhttpUpdate.update() failed: ret=");
              Serial.println(ret);
          }
        }
        else
        {
          Serial.println("But ignoring because AutoUpdate!=1.");
        }
      }
    }
  }
  else
  {
    Serial.println("Failed to download json config file.");
  }
  httpClient.end();
}
