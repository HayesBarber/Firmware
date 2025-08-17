#include "env.h"
#include <Arduino.h>
#include <AutoWiFi.h>
#include <ButtonHandler.h>
#include <GFXDriver.h>
#include <RestBeacon.h>
#include <RotaryEvents.h>
#include <TinyFetch.h>

const uint16_t HTTP_PORT = 80;
const uint16_t UDP_PORT = 4210;
const uint8_t ENCODER_CLK = 13;
const uint8_t ENCODER_DT = 10;
const uint8_t BUTTON_PIN = 14;

AutoWiFi wifi;
GFXDriver screen;
RestBeacon beacon(HTTP_PORT, UDP_PORT);
TinyFetch client;
Button button;

void udpTask(void *pvParameters) {
  while (1) {
    beacon.loopUdp();
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void onScreenTouch() {}

String onMessage(const Message &msg) {
  String action = msg.getProperty("action");
  String reply = "Unknown action";

  if (action == "uptime") {
    reply = String(millis());
  } else if (action == "version") {
    reply = VERSION;
  }

  return reply;
}

void onDiscovery(IPAddress sender, uint16_t port, const String &message) {
  if (message != DISCOVERY_PASSCODE) {
    Serial.println(
        "Received discovery message but it did not match the passcode");
    return;
  }

  Serial.println("Received discovery message from: " + sender.toString() + ":" +
                 String(port));
  String baseUrl = "http://" + sender.toString() + ":" + String(port);
  client.setBaseUrl(baseUrl);
  Serial.println("Set base URL to: " + baseUrl);

  Message msg;
  msg.addProperty("name", "Dial");
  msg.addProperty("ip", wifi.getIP().toString());
  msg.addProperty("mac", wifi.getMac());
  msg.addProperty("type", "interface");

  String jsonMessage = msg.toJson();
  Serial.println("Sending check-in message: " + jsonMessage);
  HttpResponse response = client.post("/discovery/check-in", jsonMessage);
  Serial.printf("Check-in response code: %d\n", response.statusCode);
}

void onLeftTurn() {}

void onRightTurn() {}

void onButtonPressed(int pin) {}

void setup() {
  Serial.begin(115200);
  while (!Serial)
    ;

  screen.init(onScreenTouch);

  AutoWiFi::State state = wifi.connect();
  if (state == AutoWiFi::State::AP_MODE) {
    screen.writeText("AP Mode", XL);
    return;
  }

  beacon.onMessage(onMessage);
  beacon.onDiscovery(onDiscovery);
  beacon.begin();

  RotaryEvents::getInstance().init(ENCODER_CLK, ENCODER_DT, onLeftTurn,
                                   onRightTurn, 2);

  button.init(BUTTON_PIN, onButtonPressed);

  xTaskCreatePinnedToCore(udpTask, "UdpTask", 4096, nullptr, 2, nullptr, 1);
}

void loop() {
  wifi.loop();
  if (wifi.getState() == AutoWiFi::State::AP_MODE)
    return;

  screen.loop();
  beacon.loopHttp();
  button.update();
}
