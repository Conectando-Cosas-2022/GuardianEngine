#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"

#define MSG_BUFFER_SIZE (50)

WiFiClient espClient;
PubSubClient client(espClient);

DynamicJsonDocument incoming_message(256);

void setup_wifi() {
    extern const char* wifi_ssid;
    delay(10);
    Serial.println();
    Serial.print("Conectando a: ");
    Serial.println(wifi_ssid);

    WiFi.mode(WIFI_STA); // Declarar la ESP como STATION
    WiFi.begin(wifi_ssid, wifi_password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    randomSeed(micros());

    Serial.println("");
    Serial.println("¡Conectado!");
    Serial.print("Dirección IP asignada: ");
    Serial.println(WiFi.localIP());
}
 unsigned long lastMsg = 0;  // Control de tiempo de reporte
int msgPeriod = 2000;       // Actualizar los datos cada 2 segundos

// Función de callback para recepción de mensajes MQTT 
//(Tópicos a los que está suscrita la placa)
// Se llama cada vez que arriba un mensaje entrante 
//(En este ejemplo la placa se suscribirá al tópico: 
//v1/devices/me/rpc/request/+)
void callback(char* topic, byte* payload, unsigned int length){

    // Log en Monitor Serie
  Serial.print("Mensaje recibido [");
  Serial.print(topic);
  Serial.print("]: ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // En el nombre del tópico agrega un identificador del mensaje que queremos extraer para responder solicitudes
  String _topic = String(topic);

  // Detectar de qué tópico viene el "mensaje"
  if (_topic.startsWith("v1/devices/me/rpc/request/")) { // El servidor "me pide que haga algo" (RPC)
    // Obtener el número de solicitud (request number)
    String _request_id = _topic.substring(26);

    // Leer el objeto JSON (Utilizando ArduinoJson)
    deserializeJson(incoming_message, payload); // Interpretar el cuerpo del mensaje como Json
    String metodo = incoming_message["method"]; // Obtener del objeto Json, el método RPC solicitado

    // Ejecutar una acción de acuerdo al método solicitado
  }
}

// Establecer y mantener la conexión con el servidor MQTT (En este caso de ThingsBoard)
extern const char* tb_device_token;
void reconnect() {
  // Bucle hasta lograr la conexión
  while (!client.connected()) {
    Serial.print("Intentando conectar MQTT...");
    if (client.connect("ESP8266", tb_device_token, tb_device_token)) {  //Nombre del Device y Token para conectarse
      Serial.println("¡Conectado!");
      
      // Una vez conectado, suscribirse al tópico para recibir solicitudes RPC
      client.subscribe("v1/devices/me/rpc/request/+");
      
    } else {
      
      Serial.print("Error, rc = ");
      Serial.print(client.state());
      Serial.println("Reintenar en 5 segundos...");
      // Esperar 5 segundos antes de reintentar
      delay(5000);
      
    }
  }
    
}
extern const char* tb_mqtt_server;
extern const int tb_mqtt_port;

/*========= SETUP =========*/

void setup() {
  // Conectividad
  Serial.begin(115200);                   // Inicializar conexión Serie para utilizar el Monitor
  setup_wifi();                           // Establecer la conexión WiFi
  client.setServer(tb_mqtt_server, tb_mqtt_port);// Establecer los datos para la conexión MQTT
  client.setCallback(callback);           // Establecer la función del callback para la llegada de mensajes en tópicos suscriptos

  // Sensores y actuadores
  
};

bool movement = 0;
void loop() {

  // === Conexión e intercambio de mensajes MQTT ===
  if (!client.connected()) {  // Controlar en cada ciclo la conexión con el servidor
    reconnect();              // Y recuperarla en caso de desconexión
  }
  
  client.loop();              // Controlar si hay mensajes entrantes o para enviar al servidor
     
  // === Realizar las tareas asignadas al dispositivo ===
  // En este caso se medirá temperatura y humedad para reportar periódicamente
  // El control de tiempo se hace con millis para que no sea bloqueante y en "paralelo" completar
  // ciclos del bucle principal
  
  unsigned long now = millis();
  if (now - lastMsg > msgPeriod) {
    lastMsg = now;
    
    movement = true; //dht.readMovement();  // Read movement

    // Publicar los datos en el tópio de telemetría para que el servidor los reciba
    DynamicJsonDocument resp(256);
    resp["movement"] = random(0,1); //temperature;  //Agrega el dato al Json, ej: "temperature": 21.5
    char buffer[256];
    serializeJson(resp, buffer);
    client.publish("v1/devices/me/telemetry", buffer);  // Publica el mensaje de telemetría
    
    Serial.print("Publicar mensaje [telemetry]: ");
    Serial.println(buffer);
    
  }
}
