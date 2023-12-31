#include "esp_camera.h"
#define CAMERA_MODEL_ESP32S3_EYE
#include "camera_pins.h"
#include <WiFi.h>
#include <ArduinoWebsockets.h>
#include <ESP32Servo.h>

Servo servoMotor;

const int SERVO_PIN = 2;

const int ECHO_PROX = 40;
const int TRIG_PROX = 39;
const int ECHO_LEVEL = 41;
const int TRIG_LEVEL = 42;

const int TEMP_PIN = 19;

float durationProx, distanceProx, durationLevel, distanceLevel, temp, valor;

const char* ssid = "Daniel";
const char* password = "d4t42023";

const char* websocket_server_host = "viaduct.proxy.rlwy.net";
const uint16_t websocket_server_port1 = 44691;

using namespace websockets;

WebsocketsClient client;
WebsocketsClient dataClient;
WebsocketsClient stateClient;
WebsocketsClient photoClient;

WiFiClient wifiClient;

bool sendImage = false;

void setup() {
  Serial.begin(115200);

  pinMode(LED_BUILTIN, OUTPUT);
  //pinMode(IF_PIN, INPUT);
  pinMode(ECHO_PROX, INPUT);
  pinMode(TRIG_PROX, OUTPUT);
  pinMode(ECHO_LEVEL, INPUT);
  pinMode(TRIG_LEVEL, OUTPUT);

  servoMotor.attach(SERVO_PIN);

  servoMotor.write(0);

  Serial.print("\nConnecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  if (cameraSetup() == 1) {
    Serial.print("\ncameraSetup: OK");
  } else {
    Serial.print("\ncameraSetup: ERROR");
    return;
  }

  client.onMessage(onMessageCallback);
  client.onEvent(onEventsCallback);

  dataClient.onMessage(onMessageCallback);
  dataClient.onEvent(onEventsCallback);

  stateClient.onMessage(onStateMessageCallback);
  stateClient.onEvent(onStateEventsCallback);

  photoClient.onMessage(onMessageCallback);
  photoClient.onEvent(onEventsCallback);

  while (!stateClient.connect(websocket_server_host, websocket_server_port1, "/ws")) { }
  while (!dataClient.connect(websocket_server_host, websocket_server_port1, "/data_ws")) { }
  while (!photoClient.connect(websocket_server_host, websocket_server_port1, "/photo_ws")) { }
  while (!client.connect(websocket_server_host, websocket_server_port1, "/image_ws")) { }
}

void loop() {

  stateClient.poll();
  dataClient.poll();
  client.poll();

  camera_fb_t* fb = esp_camera_fb_get();

  if (!fb) {
    esp_camera_fb_return(fb);
    Serial.println("\nesp_camera_fb_get error");
    return;
  }

  if (fb->format != PIXFORMAT_JPEG) {
    Serial.println("\nPIXFORMAT_JPEG error");
    return;
  }

  esp_camera_fb_return(fb);

  if ( distanceProx < 20) {
    
    photoClient.sendBinary((const char*)fb->buf, fb->len);
    digitalWrite(LED_BUILTIN, HIGH);
    servoMotor.write(180);
    delay(2000);
    servoMotor.write(0);
    digitalWrite(LED_BUILTIN, LOW);

  }

  if (sendImage) {
      camera_fb_t* fb = esp_camera_fb_get();

    if (!fb) {
      esp_camera_fb_return(fb);
      Serial.println("\nesp_camera_fb_get error");
      return;
    }

    if (fb->format != PIXFORMAT_JPEG) {
      Serial.println("\nPIXFORMAT_JPEG error");
      return;
    }

    client.sendBinary((const char*)fb->buf, fb->len);
    esp_camera_fb_return(fb);
  }

  US();

  delay(500);
 
}



void US()
{
  digitalWrite(TRIG_PROX, LOW);
  delayMicroseconds(5);
  digitalWrite(TRIG_PROX, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PROX, LOW);
  
  durationProx = pulseIn(ECHO_PROX, HIGH);
  distanceProx = durationProx * 0.017;
  Serial.print("Distancie Prox: ");
  Serial.print(distanceProx);
  Serial.println(" cm");

  digitalWrite(TRIG_LEVEL, LOW);
  delayMicroseconds(5);
  digitalWrite(TRIG_LEVEL, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_LEVEL, LOW);

  durationLevel = pulseIn(ECHO_LEVEL, HIGH);
  distanceLevel = durationLevel * 0.017;
  Serial.print("Distance Level: ");
  Serial.print(distanceLevel);
  Serial.println(" cm");

  // TEMPERATURA

  valor = analogRead(TEMP_PIN);
  temp = (5 * valor * 100)/1023 - 50;
  Serial.print("Valor de temperatura: ");
  Serial.println(temp);

  dataClient.send(String(distanceProx) + ";" + String(distanceLevel) + ";" + String(temp));
  
}


void onStateMessageCallback(WebsocketsMessage message) {
  Serial.println(message.data());

  String data = message.data();
  if (data.length() > 20) {
    return;
  }

  int index = data.indexOf("=");
  if (index != -1) {
    String key = data.substring(0, index);
    String value = data.substring(index + 1);

    if (key == "ON_BOARD_LED_1") {
      if (value.toInt() == 1) {
        digitalWrite(LED_BUILTIN, HIGH);
      } else {
        digitalWrite(LED_BUILTIN, LOW);
      }
    }
       
    if (key == "ON_BOARD_VIDEO_1") {
          if (value.toInt() == 1) {
            sendImage = true;
          } else {
            sendImage = false;
          }
        }
      }

}
void onStateEventsCallback(WebsocketsEvent event, String data) {
  if (event == WebsocketsEvent::ConnectionOpened) {
    Serial.println("\nConnection Opened");
  } else if (event == WebsocketsEvent::ConnectionClosed) {
    Serial.println("\nConnection Closed");
    ESP.restart();
  } else if (event == WebsocketsEvent::GotPing) {
    Serial.println("\nGot a Ping!");
  } else if (event == WebsocketsEvent::GotPong) {
    Serial.println("\nGot a Pong!");
  }
}

void onEventsCallback(WebsocketsEvent event, String data) {
  return;
}
void onMessageCallback(WebsocketsMessage message) {
  return;
}

int cameraSetup(void) {

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
  config.xclk_freq_hz = 10000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_240X240;
  config.jpeg_quality = 40;
  config.fb_count = 2;
  config.fb_location = CAMERA_FB_IN_DRAM;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return 0;
  }

  sensor_t* s = esp_camera_sensor_get();

  s->set_vflip(s, 1);
  s->set_brightness(s, 1);
  s->set_saturation(s, 0);

  Serial.println("Camera configuration complete!");
  return 1;
}
