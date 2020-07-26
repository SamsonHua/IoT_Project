//Assembled by Samson Hua
//Example code was used from the Sinric library, as well as the IR remote library

//Import required libraries
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>
#include <StreamString.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>

//Creating Wifi Related Objects
ESP8266WiFiMulti WiFiMulti;
WebSocketsClient webSocket;
WiFiClient client;

//Wifi Credentials
#define APIKey "API_KEY" //This is from the Sinric
#define SSID "NETWORK_SSID" //This is your network name
#define Password "NETWORK_PASSWORD" //This is your network password

//Digital LED Pin
#define LED_PIN D1 //Optional Status LED
#define IR_LED D2 //Have IR LED on Pin D2, feel free to change this

//Creating IR transmitter object
IRsend transmitter(IR_LED);

//Define codes
#define On_Code 0x8F330CF //Put your Hexadecimal codes in here for the IR device you want to trigger, use 0x to indicate hexadecimal

#define HEARTBEAT_INTERVAL 300000

uint64_t heartbeatTimestamp = 0;

bool isConnected = false;

//Power State
void setPowerStateOnServer(String deviceId, String value);

void setTargetTemperatureOnServer(String deviceId, String value, String scale);


//This method is when the device is turned on
void turnOn(String deviceId) {
  if (deviceId == "5f1b37dead7a48327f3766c5") // Device ID of first device
  {  
    Serial.print("Turning on LED");
    digitalWrite(LED_PIN,HIGH);
    transmitter.sendNEC(On_Code, 32);
  } 
  else {
    Serial.print("Error");  
  }     
}


//This method is when the device is turned off
void turnOff(String deviceId) {
   if (deviceId == "5f1b37dead7a48327f3766c5") // Device ID of first device
   {  
     Serial.print("Turning off LED");
     transmitter.sendNEC(On_Code, 32);
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
  transmitter.begin();

  //Declartion of Pinmodes
  pinMode(LED_PIN, OUTPUT);
  pinMode(IR_LED, OUTPUT);

  //Constructors
  WiFiMulti.addAP(SSID,Password);
  Serial.print("\n Attempting to connect to: ");
  Serial.print(SSID);

  while(WiFiMulti.run() != WL_CONNECTED) {
    digitalWrite(LED_PIN, LOW);
    delay(500);
    digitalWrite(LED_PIN, HIGH);
    Serial.print(" \n Connecting....");
  }

  //WiFi Status Messafe
  while(WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print("\nConnecting to wifi..");

  }

  //Give info message once connected:

  if(WiFiMulti.run() == WL_CONNECTED) {
    digitalWrite(LED_PIN, HIGH);
    Serial.print("\n Connected to WiFi");
    Serial.print("\n IP address: ");
    Serial.println(WiFi.localIP());
  }

  //Connect to Sinric
  webSocket.begin("iot.sinric.com", 80, "/");
  webSocket.onEvent(webSocketEvent);
  webSocket.setAuthorization("apikey", APIKey);

  webSocket.setReconnectInterval(5000); 

  Serial.print("\n Connected to Sinric");
  
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