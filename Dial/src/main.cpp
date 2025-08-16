#include <Arduino.h>
#include <AutoWiFi.h>
#include <GFXDriver.h>
#include <RestBeacon.h>

const uint16_t HTTP_PORT = 80;
const uint16_t UDP_PORT = 4210;

AutoWiFi wifi;
GFXDriver screen;
RestBeacon beacon(HTTP_PORT, UDP_PORT);

void udpTask(void *pvParameters) {
  while (1) {
    beacon.loopUdp();
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void onScreenTouch() {}

String onMessage(const Message &msg) {}

void onDiscovery(IPAddress sender, uint16_t port, const String &message) {}

void setup() {
  Serial.begin(115200);
  while (!Serial)
    ;

  AutoWiFi::State state = wifi.connect();

  if (state == AutoWiFi::State::AP_MODE) {
    return;
  }

  screen.init(onScreenTouch);

  beacon.onMessage(onMessage);
  beacon.onDiscovery(onDiscovery);
  beacon.begin();

  xTaskCreatePinnedToCore(udpTask, "UdpTask", 4096, nullptr, 2, nullptr, 1);
}

void loop() {
  wifi.loop();
  if (wifi.getState() == AutoWiFi::State::AP_MODE)
    return;

  screen.loop();
  beacon.loopHttp();
}
