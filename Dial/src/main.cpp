#include "env.h"
#include "models.h"
#include <Arduino.h>
#include <AutoWiFi.h>
#include <ButtonHandler.h>
#include <GFXDriver.h>
#include <RestBeacon.h>
#include <RotaryEvents.h>
#include <TimeKeeper.h>
#include <TinyFetch.h>
#include <vector>

const uint16_t HTTP_PORT = 80;
const uint16_t UDP_PORT = 4210;
const uint8_t ENCODER_CLK = 13;
const uint8_t ENCODER_DT = 10;
const uint8_t BUTTON_PIN = 14;
const unsigned long IDLE_THRESHOLD_MS = 20000;

TimeKeeper timeKeeper;
AutoWiFi wifi;
GFXDriver screen;
RestBeacon beacon(HTTP_PORT, UDP_PORT);
TinyFetch client;
Button button;
AppState appState;

void udpTask(void *pvParameters) {
  while (1) {
    beacon.loopUdp();
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

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

void onLeftTurn() { appState = handleEvent(appState, InputEvent::LeftTurn); }

void onRightTurn() { appState = handleEvent(appState, InputEvent::RightTurn); }

void onButtonPressed(int pin) {
  appState = handleEvent(appState, InputEvent::ButtonPress);
}

void onScreenTouch() {
  appState = handleEvent(appState, InputEvent::ScreenTouch);
}

void onIdleDetected() {
  appState = handleEvent(appState, InputEvent::IdleDetected);
}

void rotateIdleDisplay() {
  appState = handleEvent(appState, InputEvent::RotateIdleData);
}

void applyTheme(Theme &theme) {
  String colors = theme.colors;
  String body = "{ \"colors\": \"" + colors + "\" }";
  client.post("/themes/apply", body);
}

void toggleDevice(Device &device) { client.get(device.toggleUrl); }

bool shouldEnterIdle(const unsigned long lastActivityDetected,
                     UIState uiState) {
  if (uiState == UIState::Idle) {
    return false;
  }

  unsigned long currentMillis = millis();
  bool isIdle = currentMillis - lastActivityDetected >= IDLE_THRESHOLD_MS;

  return isIdle;
}

bool shouldRotateIdleData(UIState uiState, String currentTime) {
  if (uiState != UIState::Idle) {
    return false;
  }

  return timeHasChanged(currentTime);
}

bool timeHasChanged(String currentTime) {
  String newTime = timeKeeper.getTime12Hour();
  return newTime == currentTime;
}

AppState handleEvent(AppState appState, InputEvent e) {
  AppState newState = transition(appState, e);

  if (appState.uiState != newState.uiState) {
    // todo redraw
  }

  if (e == InputEvent::ScreenTouch) {
    if (appState.uiState == UIState::ShowingThemes) {
      applyTheme(newState.themes[newState.rotationIndex]);
    } else if (appState.uiState == UIState::ShowingDevices) {
      toggleDevice(newState.devices[newState.rotationIndex]);
    }
  }

  return newState;
}

AppState transition(const AppState &state, InputEvent e) {
  AppState next = state;

  switch (e) {
  case InputEvent::ButtonPress:
  case InputEvent::ScreenTouch:
  case InputEvent::LeftTurn:
  case InputEvent::RightTurn:
    next.lastActivityDetected = millis();
  }

  switch (state.uiState) {
  case UIState::Idle:
    if (e == InputEvent::LeftTurn || e == InputEvent::RightTurn ||
        e == InputEvent::ButtonPress) {
      next.uiState = UIState::ShowingDevices;
      next.rotationIndex = 0;
    } else if (e == InputEvent::RotateIdleData) {
      int totalIdleItems = 1 + (state.idleData.extras.size());
      int newIndex = (1 + state.idleData.index) % totalIdleItems;
      next.idleData.index = newIndex;
      next.idleData.time = timeKeeper.getTime12Hour();
      next.isNight = TimeKeeper::isNight(next.idleData.time);
    }
    break;

  case UIState::ShowingDevices:
    if (e == InputEvent::LeftTurn) {
      int size = next.devices.size();
      next.rotationIndex = (next.rotationIndex - 1 + size) % size;
    } else if (e == InputEvent::RightTurn) {
      int size = next.devices.size();
      next.rotationIndex = (next.rotationIndex + 1) % size;
    } else if (e == InputEvent::ButtonPress) {
      next.rotationIndex = 0;
      next.uiState = UIState::ShowingThemes;
    } else if (e == InputEvent::IdleDetected) {
      next.uiState = UIState::Idle;
      next.idleData.time = timeKeeper.getTime12Hour();
    }
    break;

  case UIState::ShowingThemes:
    if (e == InputEvent::LeftTurn) {
      int size = next.themes.size();
      next.rotationIndex = (next.rotationIndex - 1 + size) % size;
    } else if (e == InputEvent::RightTurn) {
      int size = next.themes.size();
      next.rotationIndex = (next.rotationIndex + 1) % size;
    } else if (e == InputEvent::ButtonPress) {
      next.rotationIndex = 0;
      next.uiState = UIState::ShowingDevices;
    } else if (e == InputEvent::IdleDetected) {
      next.uiState = UIState::Idle;
      next.idleData.time = timeKeeper.getTime12Hour();
    }
    break;
  }

  return next;
}

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

  if (shouldEnterIdle(appState.lastActivityDetected, appState.uiState)) {
    onIdleDetected();
  } else if (shouldRotateIdleData(appState.uiState, appState.idleData.time)) {
    rotateIdleDisplay();
  }
}
