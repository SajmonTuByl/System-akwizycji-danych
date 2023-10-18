#include <Arduino.h>
#include <WiFi.h>                         // ESP32 Wi-Fi library
#include <ArduinoJson.h>

static void WiFi_Connect();
static void SendDataToDas();
static void RunWebServer();

class SensorObj{
  public:
    int sensorSerialNo;
    String sensorName;
    String sensorStatus;
    String sensorType;
    //DateTime timeStamp;

    float sensorValue;
    String sensorUnit;
};

//-----------------------------------------------------------
// WiFi network credentials
String ssid     = "TestNetwork";
String password = "TestNetwork";
//String ssid     = "TP-LINK_C1C8";
//String password = "44498245";
//String ssid     = "Galaxy M21D80A";
//String password = "grai9133";

//-----------------------------------------------------------
// Data Acquisition Server credentials
//IPAddress serverIP(192, 168, 0, 102);
IPAddress serverIP(192, 168, 1, 100);
#define portNumber 11000
WiFiClient client_WiFi;

//-----------------------------------------------------------
// Device parameters definition
int deviceSerialNo = 1;
String deviceName = "ArduinoModule_No1";
String deviceIpAddress;
String deviceStatus = "Ok";
int deviceUpdateInterval = 4000; //miliseconds
int deviceBatteryLevel = 101;
SensorObj sensor1;

//-----------------------------------------------------------
StaticJsonDocument<600> doc;
//DynamicJsonDocument doc(600);

//-----------------------------------------------------------
// Set the Web Server port number to 80
WiFiServer server(80);
// Variable to store the HTTP request
String header;
// Auxiliar variables to store the current output state
String output25State = "off";
String output27State = "off";
// Assign output variables to GPIO pins
const int output25 = 25;
const int output27 = 27;
//-----------------------------------------------------------

void setup(){
  Serial.begin(9600);
  // Initialize the output variables as outputs
  pinMode(output25, OUTPUT);
  pinMode(output27, OUTPUT);
  // Set outputs to LOW
  digitalWrite(output25, LOW);
  digitalWrite(output27, LOW);
  
  //-----------------------------------------------------------
  // Sensor 1 definition
  sensor1.sensorSerialNo = 1;
  sensor1.sensorName = "Czujnik Temperatury 1";
  sensor1.sensorStatus = "Ok";
  sensor1.sensorType = "Temperatura";
  sensor1.sensorValue = 0;
  sensor1.sensorUnit = "oC";

  //-----------------------------------------------------------
  // Connect to Wi-Fi network with SSID and password
  WiFi_Connect();
  // and start the Web Server
  server.begin();
}

void loop(){
  // Start a local web server on a given module
  RunWebServer();

  // Check time interval
  uint32_t currentMillis = millis();
  static uint32_t lastMillis;
  const uint32_t interval = deviceUpdateInterval;
  
  // Connects to the computer, where the Data Acquisition Server (DAS) is installed
  if (!client_WiFi.connected()) {
    if (client_WiFi.connect(serverIP, portNumber)) {                                         
      Serial.print("Connected to the Data Acquisition Server IP address = "); Serial.println(serverIP);
    } else {
      Serial.print("Could NOT connect to the Data Acquisition Server IP address = "); Serial.println(serverIP);
      delay(500);
    }

    // If succesfully connected to the DAS, the following lines will be executed
  } else {
    // Receives data from the DAS and sends to the serial port
    while (client_WiFi.available()) Serial.write(client_WiFi.read());
    //while (client_A.available()) client_A.write(Serial.read());  to nie działa tak po prostu w drugą stronę
    
    // Sends the letter A (could be anything) to the DAS once every 'interval'
    if (currentMillis - lastMillis >= interval) {
        lastMillis += interval;
        //client_WiFi.print("A");
        // Start function responsible for sending data from sensors to the Data Acquisition Server
        SendDataToDas();
    }
  }
  
}

void SendDataToDas() {
  // Add values to the document to serialize
  doc["deviceSerialNo"] = deviceSerialNo;
  doc["deviceName"] = deviceName;
  doc["deviceIpAddress"] = deviceIpAddress;
  doc["deviceStatus"] = deviceStatus;
  doc["deviceUpdateInterval"] = deviceUpdateInterval;
  doc["deviceBatteryLevel"] = deviceBatteryLevel;

  JsonArray sensorsList = doc.createNestedArray("sensorsList");

  // This part of code should be replicated for every sensor (be aware of Json buffer size!)
  JsonObject obj1 = sensorsList.createNestedObject();
  obj1["sensorSerialNo"] = sensor1.sensorSerialNo;
  obj1["sensorName"] = sensor1.sensorName;
  obj1["sensorStatus"] = sensor1.sensorStatus;
  obj1["sensorType"] = sensor1.sensorType;
  obj1["sensorValue"] = sensor1.sensorValue;   //16 bitów = 11+5
  obj1["sensorUnit"] = sensor1.sensorUnit;    //18 bitów = 10+8

  //int len1 = measureJson(doc);
  //doc["len1"] = len1;

  //serializeJson(doc, Serial);
  serializeJson(doc, client_WiFi);

  doc.clear();
}


void RunWebServer(){
  //--- Ta część dotyczy WebServera ---
  WiFiClient client = server.available();   // Listen for incoming clients

  if (client) {                             // If a new client connects,
    Serial.println();
    Serial.println("--------");
    Serial.println("New Client connected through a web browser.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();

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
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            // CSS to style the on/off buttons
            // Feel free to change the background-color and font-size attributes to fit your preferences
            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println(".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;");
            client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
            client.println(".button2 {background-color: #888888;}</style></head>");

            // Web Page Heading
            client.println("<body><h1>ESP32 Web Server</h1>");

            // Display current state, and ON/OFF buttons for GPIO 25
            client.println("<p>GPIO 25 - State " + output25State + "</p>");
            // If the output25State is off, it displays the ON button
            if (output25State=="off") {
              client.println("<p><a href=\"/25/on\"><button class=\"button\">ON</button></a></p>");
            } else {
              client.println("<p><a href=\"/25/off\"><button class=\"button button2\">OFF</button></a></p>");
            }

            // Display current state, and ON/OFF buttons for GPIO 27
            client.println("<p>GPIO 27 - State " + output27State + "</p>");
            // If the output27State is off, it displays the ON button
            if (output27State=="off") {
              client.println("<p><a href=\"/27/on\"><button class=\"button\">ON</button></a></p>");
            } else {
              client.println("<p><a href=\"/27/off\"><button class=\"button button2\">OFF</button></a></p>");
            }
            client.println("</body></html>");

            // The HTTP response ends with another blank line
            client.println();
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
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("------");
}
}

void WiFi_Connect(){
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  // Print local IP address
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  deviceIpAddress = (WiFi.localIP()).toString();
}

