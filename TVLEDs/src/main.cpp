#include <Arduino.h>
#include <AutoWiFi.h>

AutoWiFi wifi;

void setup() {
  Serial.begin(115200);
  while(!Serial);

  wifi.connect();
}

void loop() {
  wifi.loop();
}
