

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <Wire.h>

#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels
#define OLED_RESET -1

const char* mqtt_server = "192.168.178.42";

WiFiClient espClient;
PubSubClient client(espClient);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// {"power":177.6,"total":368386.5,"total_returned":1869.1999999999998}
// {"power":150.78,"pf":0.66,"current":1,"voltage":232.39,"is_valid":true,"total":225240.2,"total_returned":1135.1}
// {"power":18.82,"pf":0.31,"current":0.26,"voltage":233.83,"is_valid":true,"total":70696.5,"total_returned":485.3}
// {"power":1.98,"pf":0.06,"current":0.14,"voltage":233.24,"is_valid":true,"total":72449.8,"total_returned":248.8}

StaticJsonDocument<256> doc;

char temp1[16] = "";
void drawTotal(const char* text );
void drawL1(const char* text);
void drawL2(const char* text);
void drawL3(const char* text);

void callback(char* topic, byte* payload, unsigned int length) {
  deserializeJson(doc, payload, length);
  if (strcmp(topic, "3em/total") == 0) {
    float p = doc["power"] | 0.0;
    dtostrf(p, 5, 0, temp1);
    drawTotal(temp1);
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

void reconnect() {
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

void drawTotal(const char* text) {
  display.setTextSize(3);  
  display.fillRect(0, 0, 128, 30, BLACK);
  display.setCursor(0, 0);
  display.print(text);
  display.print(" W");
  display.display();
}

void drawL1(const char* text) {
  display.setTextSize(1);
  display.fillRect(0, 35, 64, 10, BLACK);
  display.setCursor(0, 35);
  display.print("1:");
  display.print(text);
  display.print(" W");
  display.display();
}
void drawL2(const char* text) {
  display.setTextSize(1);
    display.fillRect(10, 45, 64, 10, BLACK);
  display.setCursor(0, 45);

  display.print("2:");
  display.print(text);
  display.print(" W");
  display.display();
}
void drawL3(const char* text) {
  display.setTextSize(1);
  display.fillRect(0, 55, 64, 10, BLACK);
  display.setCursor(0, 55);
  display.print("3:");
  display.print(text);
  display.print(" W");
  display.display();
}

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
  client.setCallback(callback);

  // Allow the hardware to sort itself out
  delay(1500);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}