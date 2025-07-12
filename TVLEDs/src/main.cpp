#include <Arduino.h>
#include <AutoWiFi.h>
#include <LEDStripDriver.h>
#include <RestBeacon.h>

const uint16_t NUM_PIXELS = 200;
const uint16_t BRIGHTNESS = 42;
const uint16_t DATA_PIN = 13;

AutoWiFi wifi;
LEDStripDriver stripDriver;
RestBeacon beacon;

String onMessage(const Message& msg) {}

void onDiscovery(IPAddress sender, uint16_t port) {}

void setup() {
  Serial.begin(115200);
  while(!Serial);

  wifi.connect();

  stripDriver.init<DATA_PIN>(NUM_PIXELS, BRIGHTNESS);

  beacon.onMessage(onMessage);
  beacon.onDiscovery(onDiscovery);
}

void loop() {
  wifi.loop();
  stripDriver.loop();
}
