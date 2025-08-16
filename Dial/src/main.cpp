#include <Arduino.h>
#include <AutoWiFi.h>
#include <GFXDriver.h>

AutoWiFi wifi;
GFXDriver screen;

void onScreenTouch() {}

void setup() {
  Serial.begin(115200);
  while (!Serial)
    ;

  AutoWiFi::State state = wifi.connect();

  if (state == AutoWiFi::State::AP_MODE) {
    return;
  }

  screen.init(onScreenTouch);
}

void loop() {
  wifi.loop();
  if (wifi.getState() == AutoWiFi::State::AP_MODE)
    return;

  screen.loop();
}
