/*
ESP32-CAM MQTT
*/

#include "Arduino.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "ESP32_FTPClient.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_camera.h"
#include "Base64.h"
#include "config.h"
#include "camera_pins.h"

#define MQTT_PUBLISH_TOPIC  "v1/devices/me/telemetry"
#define MQTT_SUBSCRIBE_TOPIC  "v1/devices/me/rpc/request/+"
#define CAMERA_MODEL_AI_THINKER // Has PSRAM

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
    
WiFiClient espClient;
PubSubClient client(espClient);
ESP32_FTPClient ftp (ftp_server, ftp_user, ftp_pass, 5000, 2);

DynamicJsonDocument incoming_message(256);

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

String sendImage(String id) {
  camera_fb_t * fb = NULL;
  fb = esp_camera_fb_get();  
  if(!fb) {
    Serial.println("Camera capture failed");
    return "Camera capture failed";
  }  

  /*
   * Upload to ftp server
   */
  
  Serial.println("Connecting to FTP server...");
  
  ftp.OpenConnection();  
  ftp.ChangeWorkDir(ftp_path);
  ftp.InitFile("Type I");

  String fileName = "img" + id +".jpg";
  Serial.println("Uploading "+ fileName);
  int str_len = fileName.length() + 1; 
 
  char char_array[str_len];
  fileName.toCharArray(char_array, str_len);
  
  ftp.NewFile(char_array);
  ftp.WriteData( fb->buf, fb->len );
  ftp.CloseFile();
  ftp.CloseConnection();

  esp_camera_fb_return(fb);
  return fileName;
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

    String urlDestination = sendImage(_request_id);
    String url = (String)web_url + "/" + (String)urlDestination;
    Serial.println(url);

    DynamicJsonDocument resp(1024);
    resp["url"] = url;

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
  // WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
    
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  randomSeed(micros());

  Serial.println("inicializando camara");

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
  config.frame_size = FRAMESIZE_SVGA;
  config.pixel_format = PIXFORMAT_JPEG; // for streaming
  //config.pixel_format = PIXFORMAT_RGB565; // for face detection/recognition
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;
  
  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  if(config.pixel_format == PIXFORMAT_JPEG){
    if(psramFound()){
      config.jpeg_quality = 10;
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
      // Limit the frame size when PSRAM is not available
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  } else {
    // Best option for face detection/recognition
    config.frame_size = FRAMESIZE_240X240;
#if CONFIG_IDF_TARGET_ESP32S3
    config.fb_count = 2;
#endif
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1); // flip it back
    s->set_brightness(s, 1); // up the brightness just a bit
    s->set_saturation(s, -2); // lower the saturation
  }
  // drop down frame size for higher initial frame rate
  if(config.pixel_format == PIXFORMAT_JPEG){
    s->set_framesize(s, FRAMESIZE_QVGA);
  }

#if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif

#if defined(CAMERA_MODEL_ESP32S3_EYE)
  s->set_vflip(s, 1);
#endif


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