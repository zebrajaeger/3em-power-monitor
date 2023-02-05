

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <Wire.h>
#include <time.h>

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
StaticJsonDocument<256> doc;
int16_t currentDay = -1;
float todayStartEnergy = 0;
float todayEnergy = -1;
bool dayswitch = true;
char temp1[16] = "";
// {"power":177.6,"total":368386.5,"total_returned":1869.1999999999998}
// {"power":150.78,"pf":0.66,"current":1,"voltage":232.39,"is_valid":true,"total":225240.2,"total_returned":1135.1}
// {"power":18.82,"pf":0.31,"current":0.26,"voltage":233.83,"is_valid":true,"total":70696.5,"total_returned":485.3}
// {"power":1.98,"pf":0.06,"current":0.14,"voltage":233.24,"is_valid":true,"total":72449.8,"total_returned":248.8}

void drawTotal(const char* text);
void drawL1(const char* text);
void drawL2(const char* text);
void drawL3(const char* text);
void drawTime(struct tm* time);
void mqttCallback(char* topic, byte* payload, unsigned int length);
void mqttReconnect();
void printLocalTime();
void drawEnergy(float e);

void setup() {
  Serial.begin(115200);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;  // Don't proceed, loop forever
  } else {
    Serial.println(F("Display ok"));
    display.clearDisplay();
    display.setTextColor(WHITE);
  }

  WiFiManager wifiManager;
  wifiManager.autoConnect("AutoConnectAP");

  client.setServer(mqtt_server, 1883);
  client.setCallback(mqttCallback);

  configTime(0, 0, ntpServer);
  setenv("TZ", timezone, 1);
  tzset();
  // printLocalTime();
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
        drawTime(&timeinfo);
      }
    }
  });

  // Allow the hardware to sort itself out
  delay(1500);
}

void loop() {
  if (!client.connected()) {
    mqttReconnect();
  }
  client.loop();
  timer.loop();
}

void drawTotal(const char* text) {
  display.setTextSize(3);
  display.fillRect(0, 0, 128, 21, BLACK);
  display.setCursor(2, 0);
  display.print(text);
  display.print(" W");
  display.display();
}

void drawL1(const char* text) {
  uint8_t y = 64-33;
  display.setTextSize(1);
  display.fillRect(0, y, 64, 10, BLACK);
  display.setCursor(0, y);
  display.print("L1:");
  display.print(text);
  display.print(" W");
  display.display();
}

void drawL2(const char* text) {
  uint8_t y = 64-20;
  display.setTextSize(1);
  display.fillRect(0, y, 64, 10, BLACK);
  display.setCursor(0, y);

  display.print("L2:");
  display.print(text);
  display.print(" W");
  display.display();
}

void drawL3(const char* text) {
  uint8_t y = 64-7;
  display.setTextSize(1);
  display.fillRect(0, y, 64, 7, BLACK);
  display.setCursor(0, y);
  display.print("L3:");
  display.print(text);
  display.print(" W");
  display.display();
}

void drawEnergy(float e) {
  uint8_t x = 64;
  uint8_t y = 30;
  display.setTextSize(1);
  display.fillRect(x, y, 64, 10, BLACK);
  display.setCursor(x+64-(10*6), y);
  display.printf("%6.3f", e/1000);
  display.print(" kWh");
  display.display();
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
  if (strcmp(topic, "3em/total") == 0) {
    float p = doc["power"] | 0.0;
    float e = doc["total"] | 0.0;
    if (dayswitch) {
      todayStartEnergy = e;
      dayswitch = false;
    }
      Serial.println(e);

    todayEnergy = e -= todayStartEnergy;
    dtostrf(p, 5, 0, temp1);
    drawTotal(temp1);
    drawEnergy(todayEnergy);
  }
  if (strcmp(topic, "3em/l1") == 0) {
    float p = doc["power"] | 0.0;
    dtostrf(p, 5, 0, temp1);
    drawL1(temp1);
  }
  if (strcmp(topic, "3em/l2") == 0) {
    float p = doc["power"] | 0.0;
    dtostrf(p, 5, 0, temp1);
    drawL2(temp1);
  }
  if (strcmp(topic, "3em/l3") == 0) {
    float p = doc["power"] | 0.0;
    dtostrf(p, 5, 0, temp1);
    drawL3(temp1);
  }
}

void mqttReconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("arduinoClient")) {
      Serial.println("connected");
      client.subscribe("3em/total");
      client.subscribe("3em/l1");
      client.subscribe("3em/l2");
      client.subscribe("3em/l3");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void printLocalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time 1");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S zone %Z %z ");
}