#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <Wire.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <ArduinoJson.h>

#include <PubSubClient.h>

#include <time.h>
#include <esp_sntp.h>

#include "multitimer.h"

#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels
#define OLED_RESET -1
const char* timezone = "CET-1CEST,M3.5.0,M10.5.0/3";
const char* mqtt_server = "192.168.178.42";
const char* ntpServer = "pool.ntp.org";

WiFiClient espClient;
PubSubClient client(espClient);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
MultiTimer timer;
StaticJsonDocument<512> doc;
int16_t currentDay = -1;
float todayStartEnergy = 0;
float todayEnergy = -1;
bool dayswitch = true;
char temp1[16] = "";

void drawPower(float p);
void drawPowerL1(float p);
void drawPowerL2(float p);
void drawPowerL3(float p);
void drawTime(struct tm* time);
void mqttCallback(char* topic, byte* payload, unsigned int length);
void mqttReconnect();
void drawEnergy(float e);

void setup() {
  Serial.begin(115200);

  // Display
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
  } else {
    Serial.println(F("Display ok"));
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.display();
  }

  // WiFi
  display.setCursor(0,0);
  display.print("Wifi...");
  display.display();
  WiFiManager wifiManager;
  wifiManager.autoConnect("AutoConnectAP");
  display.println("done");
  display.display();

  // MQTT
  display.print("MQTT...");
  display.display();
  client.setBufferSize(512);
  client.setServer(mqtt_server, 1883);
  client.setCallback(mqttCallback);
  display.println("done");
  display.display();

  // NTP
  display.print("NTP...");
  display.display();
  configTzTime(timezone, ntpServer);
  sntp_set_time_sync_notification_cb(
      [](struct timeval* tv) { Serial.println("NTP-Sync"); });
  sntp_set_sync_interval(1000 * 60 * 60);  // sync every 60min
  display.println("done");
  display.display();

  // Interval timer
  timer.begin();
  timer.set("time", 1000, []() {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      if (timeinfo.tm_yday != currentDay) {
        Serial.print("Dayswitch from ");
        Serial.print(currentDay);
        Serial.print(" to ");
        Serial.println(timeinfo.tm_yday);
        dayswitch = true;
        currentDay = timeinfo.tm_yday;
        Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S zone %Z %z ");
      }
      drawTime(&timeinfo);
    }
  });

  // Allow the hardware to sort itself out
  delay(1500);
}

void loop() {
  if (!client.connected()) {
    display.clearDisplay();
    display.setCursor(0,0);
    display.display();
    display.println("MQTT subscribing...");
    mqttReconnect();
    display.clearDisplay();
    display.display();
  }
  client.loop();
  timer.loop();
}

void drawPower(float p) {
  dtostrf(p, 5, 0, temp1);
  display.setTextSize(3);
  display.fillRect(0, 0, 128, 21, BLACK);
  display.setCursor(2, 0);
  display.print(temp1);
  display.print(" W");
}

void drawPowerL1(float p) {
  uint8_t y = 64 - 33;
  dtostrf(p, 5, 0, temp1);
  display.setTextSize(1);
  display.fillRect(0, y, 64, 10, BLACK);
  display.setCursor(0, y);
  display.print("L1:");
  display.print(temp1);
  display.print(" W");
}

void drawPowerL2(float p) {
  uint8_t y = 64 - 20;
  dtostrf(p, 5, 0, temp1);
  display.setTextSize(1);
  display.fillRect(0, y, 64, 10, BLACK);
  display.setCursor(0, y);
  display.print("L2:");
  display.print(temp1);
  display.print(" W");
}

void drawPowerL3(float p) {
  uint8_t y = 64 - 7;
  dtostrf(p, 5, 0, temp1);
  display.setTextSize(1);
  display.fillRect(0, y, 64, 7, BLACK);
  display.setCursor(0, y);
  display.print("L3:");
  display.print(temp1);
  display.print(" W");
}

void drawEnergy(float e) {
  uint8_t x = 64;
  uint8_t y = 30;
  display.setTextSize(1);
  display.fillRect(x, y, 64, 10, BLACK);
  display.setCursor(x + 64 - (10 * 6), y);
  display.printf("%6.3f", e / 1000);
  display.print(" kWh");
}

void drawTime(struct tm* timeinfo) {
  uint8_t y = 45;
  display.setTextSize(1);
  display.fillRect(64, y, 64, 20, BLACK);
  display.setCursor(128 - (8 * 6), y);
  display.printf("%02d:%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min,
                 timeinfo->tm_sec);
  display.setCursor(128 - (10 * 6), y + 12);
  display.printf("%02d.%02d.%02d", timeinfo->tm_mday, timeinfo->tm_mon,
                 1900 + timeinfo->tm_year);
  display.display();
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  deserializeJson(doc, payload, length);
  JsonVariant energy = doc["todayEnergy"];
  float e1 = energy["l1"];
  float e2 = energy["l2"];
  float e3 = energy["l3"];
  float e = e1 + e2 + e3;
  drawEnergy(e);

  JsonVariant power = doc["power"];
  float p1 = power["l1"];
  float p2 = power["l2"];
  float p3 = power["l3"];
  float p = p1 + p2 + p3;
  drawPower(p);
  drawPowerL1(p1);
  drawPowerL2(p2);
  drawPowerL3(p3);
  display.display();
}

void mqttReconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("3EM Power Monitor")) {
      client.subscribe("3em/display");
    } else {
      Serial.print("failed: ");
      Serial.print(client.state());
      Serial.println(" -> try again in 5 seconds");
      delay(5000);
    }
  }
  Serial.println("connected");
}
