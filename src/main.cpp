//Import required libraries
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>
#include <StreamString.h>

//Creating Wifi Related Objects
ESP8266WiFiMulti WiFiMulti;
WebSocketsClient webSocket;
WiFiClient client;

//Wifi Credentials
#define APIKey "548a84d9-f2dd-4fbc-961c-cebae4d7ccf1"
#define SSID "SHAW-43E2A0"
#define Password "2511810D2876"

#define LED_PIN D1
#define HEARTBEAT_INTERVAL 300000

uint64_t heartbeatTimestamp = 0;

bool isConnected = false;

//Power State
void setPowerStateOnServer(String deviceId, String value);

void setTargetTemperatureOnServer(String deviceId, String value, String scale);

void turnOn(String deviceId) {
  if (deviceId == "5f1b37dead7a48327f3766c5") // Device ID of first device
  {  
    Serial.print("Turning on LED");
    digitalWrite(LED_PIN,HIGH);
  } 
  else {
    Serial.print("Error");  
  }     
}

void turnOff(String deviceId) {
   if (deviceId == "5f1b37dead7a48327f3766c5") // Device ID of first device
   {  
     Serial.print("Turning off LED");
     digitalWrite(LED_PIN,LOW);
   }
  else {
     Serial.print("Error");
     
  }
}


void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      isConnected = false;    
      Serial.printf("[WSc] Webservice disconnected from sinric.com!\n");
      break;
    case WStype_CONNECTED: {
      isConnected = true;
      Serial.printf("[WSc] Service connected to sinric.com at url: %s\n", payload);
      Serial.printf("Waiting for commands from sinric.com ...\n");        
      }
      break;
    case WStype_TEXT: {
        Serial.printf("[WSc] get text: %s\n", payload);
        // Example payloads

        // For Switch or Light device types
        //{"deviceId": xxxx, "action": "setPowerState", value: "ON"} // https://developer.amazon.com/docs/device-apis/alexa-powercontroller.html

        // For Light device type
        // Look at the light example in github
          
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject((char*)payload); 
        String deviceId = json ["deviceId"];     
        String action = json ["action"];
        
        if(action == "setPowerState") { // Switch or Light
            String value = json ["value"];
            if(value == "ON") {
                turnOn(deviceId);
            } else {
                turnOff(deviceId);
            }
        }
        else if (action == "SetTargetTemperature") {
            String deviceId = json ["deviceId"];     
            String action = json ["action"];
            String value = json ["value"];
        }
        else if (action == "test") {
            Serial.println("[WSc] received test command from sinric.com");
        }
      }
      break;
    case WStype_BIN:
      Serial.printf("[WSc] get binary length: %u\n", length);
      break;
  }
}

//////////////////////////////////////////

void setup() {
  //Set up serial communication
  Serial.begin(115200);


  //Declartion of Pinmodes
  pinMode(LED_PIN, OUTPUT);

  //Constructors
  WiFiMulti.addAP(SSID,Password);
  Serial.print("\n Attempting to connect to: ");
  Serial.print(SSID);

  while(WiFiMulti.run() != WL_CONNECTED) {
    delay(500);
    Serial.print(" \n Connecting....");
  }

  //WiFi Status Messafe
  while(WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print("\nConnecting to wifi..");

  }

  //Give info message once connected:

  if(WiFiMulti.run() == WL_CONNECTED) {
    Serial.print("\n Connected to WiFi");
    Serial.print("\n IP address: ");
    Serial.println(WiFi.localIP());
  }

  //Connect to Sinric
  webSocket.begin("iot.sinric.com", 80, "/");

  webSocket.onEvent(webSocketEvent);
  webSocket.setAuthorization("apikey", APIKey);

  webSocket.setReconnectInterval(5000); 

  Serial.print("\n Sinric Connected");
  
}

void loop() {

  webSocket.loop();
  
  if(isConnected) {
      uint64_t now = millis();
      
      // Send heartbeat in order to avoid disconnections during ISP resetting IPs over night. Thanks @MacSass
      if((now - heartbeatTimestamp) > HEARTBEAT_INTERVAL) {
          heartbeatTimestamp = now;
          webSocket.sendTXT("H");          
      }
  }   

}



void setPowerStateOnServer(String deviceId, String value) {
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["deviceId"] = deviceId;
  root["action"] = "setPowerState";
  root["value"] = value;
  StreamString databuf;
  root.printTo(databuf);
  
  webSocket.sendTXT(databuf);
}

//eg: setPowerStateOnServer("deviceid", "CELSIUS", "25.0")

// Call ONLY If status changed. DO NOT CALL THIS IN loop() and overload the server. 

void setTargetTemperatureOnServer(String deviceId, String value, String scale) {
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["action"] = "SetTargetTemperature";
  root["deviceId"] = deviceId;
  
  JsonObject& valueObj = root.createNestedObject("value");
  JsonObject& targetSetpoint = valueObj.createNestedObject("targetSetpoint");
  targetSetpoint["value"] = value;
  targetSetpoint["scale"] = scale;
   
  StreamString databuf;
  root.printTo(databuf);
  
  webSocket.sendTXT(databuf);
}