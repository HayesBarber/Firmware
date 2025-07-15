#include <Arduino.h>
#include <AutoWiFi.h>
#include <LEDStripDriver.h>
#include <RestBeacon.h>
#include <Message.h>
#include <TinyFetch.h>
#include "env.h"

const uint16_t NUM_PIXELS = 200;
const uint8_t BRIGHTNESS = 42;
const uint16_t DATA_PIN = 13;
const uint16_t HTTP_PORT = 80;
const uint16_t UDP_PORT = 4210;

AutoWiFi wifi;
LEDStripDriver stripDriver;
RestBeacon beacon(HTTP_PORT, UDP_PORT);
TinyFetch client;

void udpTask(void* pvParameters) {
  while (1) {
    beacon.loopUdp();
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

String onMessage(const Message& msg) {
  String action = msg.getProperty("action");
  String reply = "Unknown action";

  if (action == "on") {
    stripDriver.on();
    reply = "Turned LEDs on";
  } else if (action == "off") {
    stripDriver.off();
    reply = "Turned LEDs off";
  } else if (action == "toggle") {
    stripDriver.toggle();
    reply = "Toggled LEDs";
  } else if (action == "powerState") {
    reply = stripDriver.getPowerState() ? "on" : "off";
  } else if (action == "fill") {
    String colors = msg.getProperty("colors");
    stripDriver.fill(colors);
    reply = "Filled LEDs";
  } else if (action == "uptime") {
    reply = String(millis());
  }

  return reply;
}

void onDiscovery(IPAddress sender, uint16_t port, const String& message) {
  if (message != DISCOVERY_PASSCODE) {
    Serial.println("Received discovery message but it did not match the passcode");
    return;
  }
  Serial.println("Received discovery message from: " + sender.toString() + ":" + String(port));
  String baseUrl = "http://" + sender.toString() + ":" + String(port);
  client.setBaseUrl(baseUrl);
  Serial.println("Set base URL to: " + baseUrl);

  Message msg;
  msg.addProperty("name", "TV LEDs");
  msg.addProperty("ip", wifi.getIP().toString());
  msg.addProperty("mac", wifi.getMac());
  msg.addProperty("type", "other");
  msg.addProperty("power_state", stripDriver.getPowerState() ? "on" : "off");

  String jsonMessage = msg.toJson();
  Serial.println("Sending check-in message: " + jsonMessage);
  client.post("/discovery/check-in", jsonMessage);
}

void setup() {
  Serial.begin(115200);
  while(!Serial);

  AutoWiFi::State state = wifi.connect();
  
  if (state == AutoWiFi::State::AP_MODE) return;

  stripDriver.init<DATA_PIN>(NUM_PIXELS, BRIGHTNESS);

  beacon.onMessage(onMessage);
  beacon.onDiscovery(onDiscovery);
  beacon.begin();

  xTaskCreatePinnedToCore(
    udpTask,
    "UdpTask",
    4096,
    nullptr,
    2,
    nullptr,
    1
  );
}

void loop() {
  wifi.loop();
  if (wifi.getState() == AutoWiFi::State::AP_MODE) return;
  beacon.loopHttp();
  stripDriver.loop();
}
