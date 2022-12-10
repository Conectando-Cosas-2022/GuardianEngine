#include "Arduino.h"
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"

#define MSG_BUFFER_SIZE (50)
#define FAN_PORT 5 // Smoke actuator
#define VAPE_PORT 4 // Smoke actuator
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

unsigned long lastMsg = 0;  // Time report control
unsigned long msgPeriod = 2000;       // Update data every 2 seconds

// Callback function for MQTT message reception
// It gets called every time there is an incoming message 
// Topic: v1/devices/me/rpc/request/+
void callback(char* topic, byte* payload, unsigned int length){

  // Log Serial Monitor
  Serial.print("Message received [");
  Serial.print(topic);
  Serial.print("]: ");
  for (unsigned int i = 0; i < length; i++) {
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

    if (method == "getCurrentState") {
      char outTopic[128];
      ("v1/devices/me/rpc/response/"+_request_id).toCharArray(outTopic,128);
      Serial.println(outTopic);

      char buffer[1024];

      // Get network scan information
      DynamicJsonDocument resp(1024);
      resp["smoke"] = digitalRead(VAPE_PORT);

      serializeJson(resp, buffer);
      Serial.println(buffer);

      Serial.println(client.publish(outTopic, buffer));
    } 
    
    if (method == "setState") {
      int state = incoming_message["params"];

      if (state == 1) {
        digitalWrite(FAN_PORT, HIGH);
        digitalWrite(VAPE_PORT, LOW);
      } else {
        digitalWrite(FAN_PORT, LOW);
        digitalWrite(VAPE_PORT, HIGH);
      }
    }  

  }
}

extern const char* tb_device_token_3;
void reconnect() {
  // Bucle hasta lograr la conexión
  while (!client.connected()) {
    Serial.print("Trying to connect MQTT...");
    if (client.connect("ESP8266", tb_device_token_3, tb_device_token_3)) {  // Name of the device and token to connect
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
  client.setBufferSize(2048); // Set Buffer size to be larger to sustain sending network json data


  // Sensors and actuators
  pinMode(FAN_PORT, OUTPUT);
  pinMode(VAPE_PORT, OUTPUT);
  digitalWrite(VAPE_PORT, HIGH);

};

void loop() {

  // === Connection and MQTT messages exchange ===
  if (!client.connected()) {  // Control in each server cycle
    reconnect();              // And get it back up in case of disconnection
  }
  
  client.loop();              // Control if there are incoming or outgoing server messages
    
};
