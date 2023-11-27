// Kod dla mikrokontrolera opartego na ESP8266 (NodeMCU) z podłączony czujnikiem temperatury i ciśnienia

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <WiFiClientSecure.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <ADS1X15.h>

//#define SDA 0
//#define SCL 2

static void SendDataToDas();
static void webSocketEvent(WStype_t type, uint8_t * payload, size_t length);

ESP8266WiFiMulti wiFiMulti;
WebSocketsClient webSocket;
ADS1115 ADS(0x48);

//-----------------------------------------------------------
// Data Acquisition Server credentials
IPAddress serverIP(192, 168, 1, 101);
#define portNumber 11000
//-----------------------------------------------------------
// Device parameters definition
int deviceId = 2;
String deviceName = "Board_No2";
String deviceIpAddress;
String deviceStatus = "Ok";
int deviceUpdateInterval = 1000; //miliseconds
int deviceBatteryLevel = 101;
//-----------------------------------------------------------
// Sensors arrays definition
int sensorId[4] = {1,2,3,4};
String sensorName[4] = {};
String sensorStatus[4] = {};
String sensorType[4] = {};
float sensorValue[4] = {};
String sensorUnit[4] = {};
//-----------------------------------------------------------
const int capacity = 6*JSON_OBJECT_SIZE(1) + 6*JSON_ARRAY_SIZE(4) + 6*4*JSON_OBJECT_SIZE(1);  //6 objects + 6 tables with 4 element + 6*4 1-element objects in arrays
StaticJsonDocument<capacity> doc;
//DynamicJsonDocument doc(capacity);
//-----------------------------------------------------------
const int ledRed = 16;
const int ledGreen = 14;
const int ledBlue = 12;

void setup(){
  Serial.begin(9600);
  Serial.setDebugOutput(true);

  Serial.println(F("\nI2C PINS"));
  Serial.print(F("\tSDA = ")); Serial.println(SDA);
  Serial.print(F("\tSCL = ")); Serial.println(SCL);

  
  //Serial.print("ADS1X15_LIB_VERSION: ");
  //Serial.println(ADS1X15_LIB_VERSION);
  ADS.begin();
  Serial.println(ADS.isConnected());
  Wire.setClock(100000);
  //Gain | Max input voltage
  //0 	  ±6.144V 	default
  //1 	  ±4.096V 	
  //2 	  ±2.048V 	
  //4 	  ±1.024V 	
  //8 	  ±0.512V 	
  //16 	  ±0.256V
  ADS.setGain(1);  // 6.144 V
  ADS.setDataRate(7);  // 7 - najszybszy  860 próbek/s
  //-----------------------------------------------------------
  // Initialize the output variables as outputs
  pinMode(ledRed, OUTPUT);
  pinMode(ledGreen, OUTPUT);
  pinMode(ledBlue, OUTPUT);
  // Set outputs to LOW
  digitalWrite(ledRed, LOW);
  digitalWrite(ledGreen, LOW);
  digitalWrite(ledBlue, LOW);
  //-----------------------------------------------------------
  // WiFi network credentials
  wiFiMulti.addAP("TestNetwork", "TestNetwork");

  // WiFi.disconnect();
  while(wiFiMulti.run() != WL_CONNECTED) {
    digitalWrite(ledRed, HIGH);
		delay(200);
    digitalWrite(ledRed, LOW);
    Serial.print(".");
	}
  Serial.flush();

  Serial.println(WiFi.SSID());
  Serial.println(WiFi.localIP());
  deviceIpAddress = WiFi.localIP().toString();
  //-----------------------------------------------------------
  // Ustawienia WebSocket
	webSocket.begin(serverIP, portNumber, "/");
	webSocket.onEvent(webSocketEvent);
	// Podstawowa autoryzacja HTTP, niewymagane
	webSocket.setAuthorization("user", "Password");
	webSocket.setReconnectInterval(5000);
  //-----------------------------------------------------------
  sensorName[0] = "Czujnik temperatury";
  sensorStatus[0] = "1";
  sensorType[0] = "Temperatura";
  sensorValue[0] = 0;
  sensorUnit[0] = "oC";

  sensorName[1] = "Czujnik ciśnienia";
  sensorStatus[1] = "1";
  sensorType[1] = "Ciśnienie";
  sensorValue[1] = 0;
  sensorUnit[1] = "bar";
}

void loop(){
  webSocket.loop();
  // Check time interval
  uint32_t currentMillis = millis();
  static uint32_t lastMillis;
  const uint32_t interval = deviceUpdateInterval;

  ADS.setMode(0);
  float f = ADS.toVoltage(1);
  sensorValue[0] = ADS.readADC(0) * f / 0.011; // 0,011V/oC
  sensorValue[1] = ADS.readADC(1) * f / 0.0132; // 0,0132V/bar

  Serial.print("ADS 0: ");
  Serial.print(sensorValue[0]);
  Serial.print("    ");
  Serial.print("ADS 1: ");
  Serial.println(sensorValue[1]);
    
  // Sends the Json string to the DAS once every 'interval'
  if (currentMillis - lastMillis >= interval) {
      lastMillis += interval;
      SendDataToDas();
  }
}

void SendDataToDas() {
  // Add values to the document to serialize
  // Device info
  doc["deviceId"] = deviceId;
  doc["deviceName"] = deviceName;
  doc["deviceIpAddress"] = deviceIpAddress;
  doc["deviceStatus"] = deviceStatus;
  doc["deviceUpdateInterval"] = deviceUpdateInterval;
  doc["deviceBatteryLevel"] = deviceBatteryLevel;

  // Arrays for sensors
  JsonArray sensorId_Json = doc.createNestedArray("sensorId");
  JsonArray sensorName_Json = doc.createNestedArray("sensorName");
  JsonArray sensorStatus_Json = doc.createNestedArray("sensorStatus");
  JsonArray sensorType_Json = doc.createNestedArray("sensorType");
  JsonArray sensorValue_Json = doc.createNestedArray("sensorValue");
  JsonArray sensorUnit_Json = doc.createNestedArray("sensorUnit");

  // Loading each Json array with values from our arrays
  for (int id : sensorId){
    sensorId_Json.add(id);
  }
  for (String name : sensorName){
    sensorName_Json.add(name);
  }
  for (String status : sensorStatus){
    sensorStatus_Json.add(status);
  }
  for (String type : sensorType){
    sensorType_Json.add(type);
  }
  for (float value : sensorValue){
    sensorValue_Json.add(value);
  }
  for (String unit : sensorUnit){
    sensorUnit_Json.add(unit);
  }
  String data;
  serializeJson(doc, data);
  webSocket.sendTXT(data);

  doc.clear();
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
	switch(type) {
		case WStype_DISCONNECTED:
			Serial.printf("[WSc] Disconnected!\n");
      digitalWrite(ledGreen, LOW);
      digitalWrite(ledBlue, HIGH);
      delay(100);
      digitalWrite(ledBlue, LOW);

			break;
		case WStype_CONNECTED:
      digitalWrite(ledGreen, HIGH);
      Serial.printf("[WSc] Connected to url: %s:%s%s\n", serverIP.toString(), (String)portNumber, payload);
			break;
		case WStype_TEXT:
			Serial.printf("[WSc] get text: %s\n", payload);
			break;
		case WStype_BIN:
			Serial.printf("[WSc] get binary length: %u\n", length);
			break;
		case WStype_ERROR:			
		case WStype_FRAGMENT_TEXT_START:
		case WStype_FRAGMENT_BIN_START:
		case WStype_FRAGMENT:
		case WStype_FRAGMENT_FIN:
			break;
	}
}