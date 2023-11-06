#include <Arduino.h>
#include <WiFi.h>                         // ESP32 Wi-Fi library
#include <WiFiMulti.h>
//#include <ESP8266WiFi.h>
//#include <ESP8266WiFiMulti.h>
#include <WiFiClientSecure.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

static void SendDataToDas();
static void RunWebServer();
static void hexdump(const void *mem, uint32_t len, uint8_t cols);
static void webSocketEvent(WStype_t type, uint8_t * payload, size_t length);

WiFiMulti wiFiMulti;
WebSocketsClient webSocket;

//-----------------------------------------------------------
// Data Acquisition Server credentials
IPAddress serverIP(192, 168, 1, 101);
//IPAddress serverIP(192, 168, 1, 66);
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
  wiFiMulti.addAP("HRTJ UniFi", "Macierz9");
  wiFiMulti.addAP("TestNetwork", "TestNetwork");
  //wiFiMulti.addAP("TP-LINK_C1C8", "44498245");
  //wiFiMulti.addAP("Galaxy M21D80A", "grai9133");

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
  sensorStatus[0] = "Ok";
  sensorType[0] = "Temperatura";
  sensorValue[0] = 0;
  sensorUnit[0] = "oC";

  //-----------------------------------------------------------
  // start the Web Server
  server.begin();
}

void loop(){
  webSocket.loop();

  RunWebServer();
  
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


void RunWebServer(){
  /*
  //--- Ta część dotyczy WebServera ---
  WiFiClient clientWebServer = server.available();   // Listen for incoming clients

  if (clientWebServer) {                             // If a new client connects,
    Serial.println();
    Serial.println("--------");
    Serial.println("New Client connected through a web browser.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (clientWebServer.connected()) {            // loop while the client's connected
      if (clientWebServer.available()) {             // if there's bytes to read from the client,
        char c = clientWebServer.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            clientWebServer.println("HTTP/1.1 200 OK");
            clientWebServer.println("Content-type:text/html");
            clientWebServer.println("Connection: close");
            clientWebServer.println();

            // turns the GPIOs on and off
            if (header.indexOf("GET /25/on") >= 0) {
              Serial.println("GPIO 25 on");
              output25State = "on";
              digitalWrite(output25, HIGH);
            } else if (header.indexOf("GET /25/off") >= 0) {
              Serial.println("GPIO 25 off");
              output25State = "off";
              digitalWrite(output25, LOW);
            } else if (header.indexOf("GET /27/on") >= 0) {
              Serial.println("GPIO 27 on");
              output27State = "on";
              digitalWrite(output27, HIGH);
            } else if (header.indexOf("GET /27/off") >= 0) {
              Serial.println("GPIO 27 off");
              output27State = "off";
              digitalWrite(output27, LOW);
            }

            // Display the HTML web page
            clientWebServer.println("<!DOCTYPE html><html>");
            clientWebServer.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            clientWebServer.println("<link rel=\"icon\" href=\"data:,\">");
            // CSS to style the on/off buttons
            // Feel free to change the background-color and font-size attributes to fit your preferences
            clientWebServer.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            clientWebServer.println(".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;");
            clientWebServer.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
            clientWebServer.println(".button2 {background-color: #888888;}</style></head>");

            // Web Page Heading
            clientWebServer.println("<body><h1>ESP32 Web Server</h1>");

            // Display current state, and ON/OFF buttons for GPIO 25
            clientWebServer.println("<p>GPIO 25 - State " + output25State + "</p>");
            // If the output25State is off, it displays the ON button
            if (output25State=="off") {
              clientWebServer.println("<p><a href=\"/25/on\"><button class=\"button\">ON</button></a></p>");
            } else {
              clientWebServer.println("<p><a href=\"/25/off\"><button class=\"button button2\">OFF</button></a></p>");
            }

            // Display current state, and ON/OFF buttons for GPIO 27
            clientWebServer.println("<p>GPIO 27 - State " + output27State + "</p>");
            // If the output27State is off, it displays the ON button
            if (output27State=="off") {
              clientWebServer.println("<p><a href=\"/27/on\"><button class=\"button\">ON</button></a></p>");
            } else {
              clientWebServer.println("<p><a href=\"/27/off\"><button class=\"button button2\">OFF</button></a></p>");
            }
            clientWebServer.println("</body></html>");

            // The HTTP response ends with another blank line
            clientWebServer.println();
            // Break out of the while loop
            break;
          } else { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    clientWebServer.stop();
    Serial.println("Client disconnected.");
    Serial.println("------");
    
  }
*/
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
      digitalWrite(ledGreen, LOW);
      digitalWrite(ledBlue, HIGH);
      delay(10);
      digitalWrite(ledBlue, LOW);
			break;
		case WStype_CONNECTED:
      digitalWrite(ledGreen, HIGH);
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
}