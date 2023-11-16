#include <Arduino.h>
#include <WiFi.h>                         // ESP32 Wi-Fi library
#include <WiFiMulti.h>
//#include <ESP8266WiFi.h>
//#include <ESP8266WiFiMulti.h>
#include <WiFiClientSecure.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include "ADS1X15.h"

//#define RGB_BUILTIN   48
#define RGB_BRIGHTNESS 3

static void SendDataToDas();
static void RunWebServer();
static void hexdump(const void *mem, uint32_t len, uint8_t cols);
static void webSocketEvent(WStype_t type, uint8_t * payload, size_t length);

WiFiMulti wiFiMulti;
WebSocketsClient webSocket;

ADS1115 ADS(0x48);

//-----------------------------------------------------------
// Data Acquisition Server credentials
//IPAddress serverIP(192, 168, 1, 101);
IPAddress serverIP(192, 168, 1, 69);

//IPAddress serverIP(192, 168, 60, 84);
//IPAddress serverIP(192, 168, 1, 135);
#define portNumber 11000

//-----------------------------------------------------------
// Device parameters definition
int deviceId = 1;
String deviceName = "ArduinoModule_No1";
String deviceIpAddress;
String deviceStatus = "Ok";
int deviceUpdateInterval = 4000; //miliseconds
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
// Set the Web Server port number to 80
WiFiServer server(80);
// Variable to store the HTTP request
String header;
// Auxiliar variables to store the current output state
String output25State = "off";
String output27State = "off";
// Assign output variables to GPIO pins
const int ledRed = 18;
const int ledGreen = 19;
const int ledBlue = 23;
//-----------------------------------------------------------

void setup(){
  Serial.begin(9600);
  Serial.setDebugOutput(true);

  setupAds();
/*
  //-----------------------------------------------------------
  // Initialize the output variables as outputs
  pinMode(ledRed, OUTPUT);
  pinMode(ledGreen, OUTPUT);
  pinMode(ledBlue, OUTPUT);
  // Set outputs to LOW
  digitalWrite(ledRed, LOW);
  digitalWrite(ledGreen, LOW);
  digitalWrite(ledBlue, LOW);
*/
  //-----------------------------------------------------------
  // WiFi network credentials
  wiFiMulti.addAP("HRTJ UniFi", "Macierz9");
  wiFiMulti.addAP("TestNetwork", "TestNetwork");
  //wiFiMulti.addAP("TP-LINK_C1C8", "44498245");
  //wiFiMulti.addAP("Galaxy M21D80A", "grai9133");

  // WiFi.disconnect();
  while(wiFiMulti.run() != WL_CONNECTED) {
    digitalWrite(ledRed, HIGH);
    neopixelWrite(RGB_BUILTIN,RGB_BRIGHTNESS,0,0); // Red
		delay(200);
    digitalWrite(RGB_BUILTIN, LOW);    // Turn the RGB LED off
    digitalWrite(ledRed, LOW);
    Serial.print(".");
	}
  Serial.flush();

  Serial.println(WiFi.SSID());
  Serial.println(WiFi.localIP());
  deviceIpAddress = WiFi.localIP().toString();

  //-----------------------------------------------------------
  // Setting WebSocket
  // server address, port and URL
	webSocket.begin(serverIP, portNumber, "/");
	// event handler
	webSocket.onEvent(webSocketEvent);
	// use HTTP Basic Authorization this is optional remove if not needed
	webSocket.setAuthorization("user", "Password");
	// try ever 5000 again if connection has failed
	webSocket.setReconnectInterval(5000);
  
  sensorName[0] = "Czujnik Temperatury 1";
  sensorStatus[0] = "1";
  sensorType[0] = "Temperatura";
  sensorValue[0] = 0;
  sensorUnit[0] = "oC";

  //-----------------------------------------------------------
  // start the Web Server
  server.begin();
}

void loop(){
  webSocket.loop();
  
  // Check time interval
  uint32_t currentMillis = millis();
  static uint32_t lastMillis;
  const uint32_t interval = deviceUpdateInterval;
    
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
  
  //int len1 = measureJson(doc);
  //doc["len1"] = len1;

  //serializeJson(doc, Serial);
  String data;
  serializeJson(doc, data);
  webSocket.sendTXT(data);

  doc.clear();
}

void hexdump(const void *mem, uint32_t len, uint8_t cols = 16) {
	const uint8_t* src = (const uint8_t*) mem;
	Serial.printf("\n[HEXDUMP] Address: 0x%08X len: 0x%X (%d)", (ptrdiff_t)src, len, len);
	for(uint32_t i = 0; i < len; i++) {
		if(i % cols == 0) {
			Serial.printf("\n[0x%08X] 0x%08X: ", (ptrdiff_t)src, i);
		}
		Serial.printf("%02X ", *src);
		src++;
	}
	Serial.printf("\n");
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
	switch(type) {
		case WStype_DISCONNECTED:
			Serial.printf("[WSc] Disconnected!\n");
      //neopixelWrite(RGB_BUILTIN,0,0,0);
      //digitalWrite(ledGreen, LOW);
      //digitalWrite(ledBlue, HIGH);
      neopixelWrite(RGB_BUILTIN,0,0,RGB_BRIGHTNESS); // Blue
      delay(50);
      //digitalWrite(ledBlue, LOW);
      neopixelWrite(RGB_BUILTIN,0,0,0);
			break;
		case WStype_CONNECTED:
      digitalWrite(ledGreen, HIGH);
      neopixelWrite(RGB_BUILTIN,0,RGB_BRIGHTNESS,0); // Green
			//Serial.printf("[WSc] Connected to url: %s:%s%s\n", serverIP.toString(), portNumber, payload);
      Serial.printf("[WSc] Connected to url: %s:%s%s\n", serverIP.toString(), (String)portNumber, payload);

			// send message to server when Connected
			//webSocket.sendTXT("Connected");
			break;
		case WStype_TEXT:
			Serial.printf("[WSc] get text: %s\n", payload);

			// send message to server
			// webSocket.sendTXT("message here");
			break;
		case WStype_BIN:
			Serial.printf("[WSc] get binary length: %u\n", length);
			hexdump(payload, length);

			// send data to server
			// webSocket.sendBIN(payload, length);
			break;
		case WStype_ERROR:			
		case WStype_FRAGMENT_TEXT_START:
		case WStype_FRAGMENT_BIN_START:
		case WStype_FRAGMENT:
		case WStype_FRAGMENT_FIN:
			break;
	}

  void setupAds()
  {
    Serial.print("ADS1X15_LIB_VERSION: ");
    Serial.println(ADS1X15_LIB_VERSION);
    ADS.begin();
    Wire.setClock(100000);
    ADS.setGain(0);  // 6.144 volt
    //  0 = slow   4 = medium   7 = fast
    ADS.setDataRate(7);
    //results = ads.readADC_Differential_0_1();

    //max 860 sps
    //SPI interface like ADS5484
  }

  void readAds()
  {
    int x;
    
    ADS.setMode(1);
    x = ADS.readADC(0);

    ADS.setMode(0);
    x = ADS.getValue();
}