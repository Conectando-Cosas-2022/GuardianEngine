#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <config.h>

#define MSG_BUFFER_SIZE (50)

WiFiClient espClient;
PubSubClient client(espClient);

DynamicJsonDocument incoming_message(256);

void setup_wifi() {

    delay(10);
    Serial.println();
    Serial.print("Conectando a: ");
    Serial.println(wifi_ssid);
}

