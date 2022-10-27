/*
ESP32-CAM MQTT
*/

#include "Arduino.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_camera.h"
#include "Base64.h"
#include "config.h"

const char* ssid = "HUAWEI-IoT";
const char* password = "ORTWiFiIoT";

const char* mqtt_server = "demo.thingsboard.io";
const unsigned int mqtt_port = 1883;
#define MQTT_USER               ""
#define MQTT_PASSWORD           ""
#define MQTT_PUBLISH_TOPIC    "v1/devices/me/telemetry"
#define MQTT_SUBSCRIBE_TOPIC    "v1/devices/me/rpc/request/+"
#define ACCESS_TOKEN "vggG6d0lMBg8QPIzN4FX"
    
WiFiClient espClient;
PubSubClient client(espClient);

DynamicJsonDocument incoming_message(256);

//ESP32-CAM 
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

void initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  //
  // WARNING!!! PSRAM IC required for UXGA resolution and high JPEG quality
  //            Ensure ESP32 Wrover Module or other board with PSRAM is selected
  //            Partial images will be transmitted if image exceeds buffer size
  //   
  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  if(psramFound()){  //PSRAM(Psuedo SRAM)IC
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

 
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    ESP.restart();
  }

  sensor_t * s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1); // flip it back
    s->set_brightness(s, 1); // up the brightness just a bit
    s->set_saturation(s, -2); // lower the saturation
  }
  // drop down frame size for higher initial frame rate
  s->set_framesize(s, FRAMESIZE_QVGA);    // SVGA(800x600), VGA(640x480), CIF(400x296), QVGA(320x240), HQVGA(240x176), QQVGA(160x120), QXGA(2048x1564 for OV3660)

  //s->set_vflip(s, 1);
  //s->set_hmirror(s, 1);

  //(GPIO4)
  ledcAttachPin(4, 4);  
  ledcSetup(4, 5000, 8);
}

void initWiFi() {
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

String sendImage(String outTopic) {
  camera_fb_t * fb = NULL;
  fb = esp_camera_fb_get();  
  if(!fb) {
    Serial.println("Camera capture failed");
    return "Camera capture failed";
  }  

  char *input = (char *)fb->buf;
  char output[base64_enc_len(3)];
  String imageFile = "data:image/jpeg;base64,";
  for (int i=0;i<fb->len;i++) {
    base64_encode(output, (input++), 3);
    if (i%3==0) imageFile += String(output);
  }
  int fbLen = imageFile.length();
  
  String clientId = "ESP32-";
  clientId += String(random(0xffff), HEX);
  if (client.connect(clientId.c_str(), ACCESS_TOKEN, ACCESS_TOKEN)) {
    
    // client.beginPublish(outTopic, fbLen, true);

    // String str = "";
    // for (size_t n=0;n<fbLen;n=n+2048) {
    //   if (n+2048<fbLen) {
    //     str = imageFile.substring(n, n+2048);
    //     client.write((uint8_t*)str.c_str(), 2048);
    //   }
    //   else if (fbLen%2048>0) {
    //     size_t remainder = fbLen%2048;
    //     str = imageFile.substring(n, n+remainder);
    //     client.write((uint8_t*)str.c_str(), remainder);
    //   }
    // }  
    
    // client.endPublish();
    
    esp_camera_fb_return(fb);
    
    return imageFile;
  }
  esp_camera_fb_return(fb);
  return "failed, rc="+client.state();
}

void callback(char* topic, byte* payload, unsigned int length) {
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
  // (request number)
  String _request_id = _topic.substring(26);
  
  // Read JSON object (Using ArduinoJson)
  deserializeJson(incoming_message, payload); // Interpreting JSON body
  String method = incoming_message["method"]; // Obtain RCP method requested
  
  // Excecute requested method
  if (method == "imageRequest") {
    char outTopic[128];
    ("v1/devices/me/rpc/response/"+_request_id).toCharArray(outTopic,128);
    Serial.println(outTopic);

    sendImage(outTopic);
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP32CAM", ACCESS_TOKEN, ACCESS_TOKEN)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      //client.publish(MQTT_PUBLISH_TOPIC, "hello world");
      // ... and resubscribe
      client.subscribe(MQTT_SUBSCRIBE_TOPIC);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
    
  Serial.begin(115200);
  randomSeed(micros());

  initCamera();
  initWiFi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  client.setBufferSize(2048);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}