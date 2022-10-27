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

#define MQTT_PUBLISH_TOPIC  "v1/devices/me/telemetry"
#define MQTT_SUBSCRIBE_TOPIC  "v1/devices/me/rpc/request/+"
    
WiFiClient espClient;
PubSubClient client(espClient);

DynamicJsonDocument incoming_message(256);

//ESP32-CAM 
#define CAM_PIN_PWDN 32
#define CAM_PIN_RESET -1 //software reset will be performed
#define CAM_PIN_XCLK 0
#define CAM_PIN_SIOD 26
#define CAM_PIN_SIOC 27

#define CAM_PIN_D7 35
#define CAM_PIN_D6 34
#define CAM_PIN_D5 39
#define CAM_PIN_D4 36
#define CAM_PIN_D3 21
#define CAM_PIN_D2 19
#define CAM_PIN_D1 18
#define CAM_PIN_D0 5
#define CAM_PIN_VSYNC 25
#define CAM_PIN_HREF 23
#define CAM_PIN_PCLK 22

static camera_config_t config = {
    .pin_pwdn = CAM_PIN_PWDN,
    .pin_reset = CAM_PIN_RESET,
    .pin_xclk = CAM_PIN_XCLK,
    .pin_sccb_sda = CAM_PIN_SIOD,
    .pin_sccb_scl = CAM_PIN_SIOC,

    .pin_d7 = CAM_PIN_D7,
    .pin_d6 = CAM_PIN_D6,
    .pin_d5 = CAM_PIN_D5,
    .pin_d4 = CAM_PIN_D4,
    .pin_d3 = CAM_PIN_D3,
    .pin_d2 = CAM_PIN_D2,
    .pin_d1 = CAM_PIN_D1,
    .pin_d0 = CAM_PIN_D0,
    .pin_vsync = CAM_PIN_VSYNC,
    .pin_href = CAM_PIN_HREF,
    .pin_pclk = CAM_PIN_PCLK,
    .xclk_freq_hz = 20000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,
    .pixel_format = PIXFORMAT_RGB565,
    .frame_size = FRAMESIZE_QVGA,
    .jpeg_quality = 12,
    .fb_count = 1,
    .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
};

void initCamera() {
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

String sendImage() {
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
  if (client.connect(clientId.c_str(), tb_device_token_cam, tb_device_token_cam)) {
    
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

    DynamicJsonDocument resp(1024);
    resp["data"] = sendImage();

    char buffer[1024];
    serializeJson(resp, buffer);
    Serial.println(buffer);

    Serial.println(client.publish(outTopic, buffer));
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP32CAM", tb_device_token_cam, tb_device_token_cam)) {
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
  client.setServer(tb_mqtt_server, tb_mqtt_port);
  client.setCallback(callback);
  client.setBufferSize(2048);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}