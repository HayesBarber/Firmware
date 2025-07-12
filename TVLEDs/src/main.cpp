#include <Arduino.h>
#include <AutoWiFi.h>
#include <LEDStripDriver.h>
#include <RestBeacon.h>
#include <TinyFetch.h>
#include "env.h"

const uint16_t NUM_PIXELS = 200;
const uint8_t BRIGHTNESS = 42;
const uint16_t DATA_PIN = 13;

AutoWiFi wifi;
LEDStripDriver stripDriver;
RestBeacon beacon(80, 4210, DISCOVERY_PASSCODE);
TinyFetch client(BASE_URI);

void beaconTask(void* pvParameters) {
  while (1) {
    beacon.loop();
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
    reply = stripDriver.getPowerState() ? "ON" : "OFF";
  } else if (action == "fill") {
    String colors = msg.getProperty("colors");
    stripDriver.fill(colors);
    reply = "Filled LEDs";
  }

  return reply;
}

void onDiscovery(IPAddress sender, uint16_t port) {
  Serial.println(sender);
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
    beaconTask,
    "BeaconTask",
    4096,
    nullptr,
    1,
    nullptr,
    1
  );
}

void loop() {
  wifi.loop();
  if (wifi.getState() == AutoWiFi::State::AP_MODE) return;
  stripDriver.loop();
}
