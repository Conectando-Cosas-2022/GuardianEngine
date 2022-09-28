#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"

#define MSG_BUFFER_SIZE (50)
#define PIR_PORT 5

WiFiClient espClient;
PubSubClient client(espClient);

DynamicJsonDocument incoming_message(256);

void setup_wifi() {
    extern const char* wifi_ssid;
    delay(10);
    Serial.println();
    Serial.print("Connecting to: ");
    Serial.println(wifi_ssid);

    WiFi.mode(WIFI_STA); // Declare the ESP as STATION
    WiFi.begin(wifi_ssid, wifi_password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    randomSeed(micros());

    Serial.println("");
    Serial.println("Connected!");
    Serial.print("Ip address assigned: ");
    Serial.println(WiFi.localIP());
}

String getNetworkScanInfo() {
  Serial.println("Scan start");
  // WiFi.scanNetworks will return the number of networks found
  int n = WiFi.scanNetworks();
  Serial.println("Scan done");
  if (n == 0)
    Serial.println("No networks found");
  else {
    Serial.print(n);
    Serial.println(" networks found...");

    // Print out the formatted json...
    Serial.println("\"wifiAccessPoints\": [");
    for (int i = 0; i < n; ++i) {
      Serial.println("{");
      Serial.print("\"macAddress\" : \"");    
      Serial.print(WiFi.BSSIDstr(i));
      Serial.println("\",");
      Serial.print("\"signalStrength\": ");     
      Serial.println(WiFi.RSSI(i));
      if(i<n-1)
      {
      Serial.println("},");
      }
      else
      {
      Serial.println("}");  
      } 
    }
    
    Serial.println("]");
    Serial.println("}");   
    Serial.println(" ");
  }    

  // Now build the jsonString
  String jsonString = "{\n";
  jsonString +="\"wifiAccessPoints\": [\n";
    for (int j = 0; j < n; ++j)
    {
      jsonString +="{\n";
      jsonString +="\"macAddress\" : \"";    
      jsonString +=(WiFi.BSSIDstr(j));
      jsonString +="\",\n";
      jsonString +="\"signalStrength\": ";     
      jsonString +=WiFi.RSSI(j);
      jsonString +="\n";
      if(j<n-1)
      {
      jsonString +="},\n";
      }
      else
      {
      jsonString +="}\n";  
      }
    }
    jsonString +=("]\n");
    jsonString +=("}\n"); 

  return jsonString;
}

unsigned long lastMsg = 0;  // Time report control
int msgPeriod = 2000;       // Update data every 2 seconds

// Callback function for MQTT message reception
// It gets called every time there is an incoming message 
// Topic: v1/devices/me/rpc/request/+
void callback(char* topic, byte* payload, unsigned int length){

  // Log Serial Monitor
  Serial.print("Message received [");
  Serial.print(topic);
  Serial.print("]: ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // In the topic name it adds a message identifier that we want to extract to respond to requests
  String _topic = String(topic);

  // Detect from which topic the message comes
  if (_topic.startsWith("v1/devices/me/rpc/request/")) { // The server "asks me to do something" (RPC)
    // (request number)
    String _request_id = _topic.substring(26);

    // Read JSON object (Using ArduinoJson)
    deserializeJson(incoming_message, payload); // Interpreting JSON body
    String method = incoming_message["method"]; // Obtain RCP method requested
    
    // Excecute requested method
    if (method == "getNetworkInformation") { 
      char outTopic[128];
      ("v1/devices/me/rpc/response/"+_request_id).toCharArray(outTopic,128);

      // Get network scan information
      DynamicJsonDocument resp(256);
      resp["data"] = getNetworkScanInfo();

      char buffer[256];
      serializeJson(resp, buffer);

      client.publish(outTopic, buffer);
    }
  }
}

// Establish and maintain connection with the MQTT Server (ThingsBoard)
extern const char* tb_device_token;
void reconnect() {
  // Bucle hasta lograr la conexión
  while (!client.connected()) {
    Serial.print("Trying to connect MQTT...");
    if (client.connect("ESP8266", tb_device_token, tb_device_token)) {  // Name of the device and token to connect
      Serial.println("Connected!");
      
      // Once connected, subscribe to the topic to receive RCP requests
      client.subscribe("v1/devices/me/rpc/request/+");
      
    } else {
      
      Serial.print("Error, rc = ");
      Serial.print(client.state());
      Serial.println("Try again in 5 seconds...");
      // Wait 5 seconds to try again
      delay(5000);
      
    }
  }
    
}
extern const char* tb_mqtt_server;
extern const int tb_mqtt_port;

/*========= SETUP =========*/

void setup() {
  // Connectivity
  Serial.begin(115200);                   // Initialize Serial connector to utilize Monitor
  setup_wifi();                           // Establish WiFi connection
  client.setServer(tb_mqtt_server, tb_mqtt_port); // Establish data for MQTT connection
  client.setCallback(callback);           // Establish callback function for topic requests

  // Sensors and actuators
  pinMode(PIR_PORT, INPUT);

};

bool movement = 0;
void loop() {

  // === Connection and MQTT messages exchange ===
  if (!client.connected()) {  // Control in each server cycle
    reconnect();              // And get it back up in case of disconnection
  }
  
  client.loop();              // Control if there are incoming or outgoing server messages
     
  // === Do assigned tasks for the board ===
  
  unsigned long now = millis();
  if (now - lastMsg > msgPeriod) {
    lastMsg = now;
    
    movement = true;  // Read movement

    // Publish the data into the telemetry topic so the server can receive them
    DynamicJsonDocument resp(256);
    resp["movement"] = digitalRead(PIR_PORT);  // Add data to JSON
    char buffer[256];
    serializeJson(resp, buffer);
    client.publish("v1/devices/me/telemetry", buffer);  // Publish telemetry message
    
    Serial.print("Publish message [telemetry]: ");
    Serial.println(buffer);
    
  }
}
