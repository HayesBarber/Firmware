#include <Arduino.h>
#include <AutoWiFi.h>
#include <LEDStripDriver.h>

const uint16_t NUM_PIXELS = 200;
const uint16_t BRIGHTNESS = 42;
const uint16_t DATA_PIN = 13;

AutoWiFi wifi;
LEDStripDriver stripDriver;

void setup() {
  Serial.begin(115200);
  while(!Serial);

  wifi.connect();

  stripDriver.init<DATA_PIN>(NUM_PIXELS, BRIGHTNESS);
}

void loop() {
  wifi.loop();
  stripDriver.loop();
}
