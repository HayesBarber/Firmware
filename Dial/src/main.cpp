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

#define Serial USBSerial

AppState transition(const AppState &state, const InputEvent e);
AppState fromIdle(const AppState &state, const InputEvent e);
AppState fromShowingDevices(const AppState &state, const InputEvent e);
AppState fromShowingThemes(const AppState &state, const InputEvent e);
void applyTheme(const Theme &theme);
void toggleDevice(const Device &device);
bool shouldEnterIdle(const unsigned long lastActivityDetected,
                     const UIState uiState);
bool timeHasChanged(const String currentTime);
bool shouldRotateIdleData(const UIState uiState, const String currentTime);
void onLeftTurn();
void onRightTurn();
void onButtonPressed(int pin);
void onScreenTouch();
void onIdleDetected();
void rotateIdleDisplay();
void showTheme(const Theme &theme);
void showDevice(const Device &device);
AppState enterIdle(AppState state);

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
AppState appState = AppState::makeInitialAppState();

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
  } else if (action == "checkIn") {
    checkIn();
    reply = "Checked in";
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

  checkIn();
}

void checkIn() {
  Message req;
  req.addProperty("name", "Dial");
  req.addProperty("ip", wifi.getIP().toString());
  req.addProperty("mac", wifi.getMac());
  req.addProperty("type", "interface");
  req.addProperty("return_response", "true");

  String jsonMessage = req.toJson();
  Serial.println("Sending check-in message: " + jsonMessage);
  HttpResponse response = client.post("/discovery/check-in", jsonMessage);

  Message rsp;
  bool valid = Message::fromJson(response.payload, rsp);
  Serial.printf("Check-in response code: %d\n", response.statusCode);
  if (!valid) {
    Serial.println("JSON response from check in was not valid");
    return;
  }

  appState = AppState::fromCheckinResponse(rsp, appState, screen);
  timeKeeper.setEpochSeconds(rsp.getProperty("epoch_time_seconds"));
}

void applyTheme(const Theme &theme) {
  String colors = theme.colors;
  String body = "{ \"colors\": \"" + colors + "\" }";
  client.post("/themes/apply", body);
}

void toggleDevice(const Device &device) { client.get(device.toggleUrl); }

bool shouldEnterIdle(const unsigned long lastActivityDetected,
                     const UIState uiState) {
  if (uiState == UIState::Idle) {
    return false;
  }

  unsigned long currentMillis = millis();
  bool isIdle = currentMillis - lastActivityDetected >= IDLE_THRESHOLD_MS;

  return isIdle;
}

bool timeHasChanged(const String currentTime) {
  String newTime = timeKeeper.getTime12Hour();
  return newTime != currentTime;
}

bool shouldRotateIdleData(const UIState uiState, const String currentTime) {
  if (uiState != UIState::Idle) {
    return false;
  }

  return timeHasChanged(currentTime);
}

AppState fromIdle(const AppState &state, const InputEvent e) {
  AppState next = state;

  if (e == InputEvent::LeftTurn || e == InputEvent::RightTurn ||
      e == InputEvent::ButtonPress || e == InputEvent::ScreenTouch) {
    next.uiState = UIState::ShowingDevices;
    next.rotationIndex = 0;
    Device current = next.devices[next.rotationIndex];
    showDevice(current);
    screen.drawPowerSymbol(LOWER_THIRD);
    return next;
  }

  if (e == InputEvent::RotateIdleData) {
    int totalIdleItems = 1 + (next.idleData.extras.size());
    int newIndex = (1 + next.idleData.index) % totalIdleItems;
    next.idleData.index = newIndex;
    next.idleData.time = timeKeeper.getTime12Hour();
    bool isNight = TimeKeeper::isNight(next.idleData.time);
    if (isNight) {
      screen.off();
    } else {
      screen.clearThird(UPPER_THIRD);
      screen.clearThird(LOWER_THIRD);

      if (next.idleData.index == 0) {
        screen.writeText(next.idleData.time, MIDDLE_THIRD, XL);
      } else {
        TextSize textSize = L;
        if (next.idleData.index - 1 < next.idleData.extrasFontSizes.size()) {
          textSize = next.idleData.extrasFontSizes[next.idleData.index - 1];
        }
        screen.writeText(next.idleData.extras[next.idleData.index - 1],
                         MIDDLE_THIRD, textSize);
      }
    }
    return next;
  }

  return next;
}

AppState fromShowingDevices(const AppState &state, const InputEvent e) {
  AppState next = state;

  if (e == InputEvent::LeftTurn) {
    int size = next.devices.size();
    next.rotationIndex = (next.rotationIndex - 1 + size) % size;
    Device current = next.devices[next.rotationIndex];
    showDevice(current);
  } else if (e == InputEvent::RightTurn) {
    int size = next.devices.size();
    next.rotationIndex = (next.rotationIndex + 1) % size;
    Device current = next.devices[next.rotationIndex];
    showDevice(current);
  } else if (e == InputEvent::ButtonPress) {
    next.rotationIndex = 0;
    next.uiState = UIState::ShowingThemes;
    Theme current = next.themes[next.rotationIndex];
    screen.writeText("Apply", LOWER_THIRD, S);
    showTheme(current);
  } else if (e == InputEvent::ScreenTouch) {
    toggleDevice(next.devices[next.rotationIndex]);
  } else if (e == InputEvent::IdleDetected) {
    return enterIdle(next);
  }

  return next;
}

AppState fromShowingThemes(const AppState &state, const InputEvent e) {
  AppState next = state;

  if (e == InputEvent::LeftTurn) {
    int size = next.themes.size();
    next.rotationIndex = (next.rotationIndex - 1 + size) % size;
    Theme current = next.themes[next.rotationIndex];
    showTheme(current);
  } else if (e == InputEvent::RightTurn) {
    int size = next.themes.size();
    next.rotationIndex = (next.rotationIndex + 1) % size;
    Theme current = next.themes[next.rotationIndex];
    showTheme(current);
  } else if (e == InputEvent::ButtonPress) {
    next.rotationIndex = 0;
    next.uiState = UIState::ShowingDevices;
    Device current = next.devices[next.rotationIndex];
    screen.clearThird(UPPER_THIRD);
    screen.drawPowerSymbol(LOWER_THIRD);
    showDevice(current);
  } else if (e == InputEvent::ScreenTouch) {
    applyTheme(next.themes[next.rotationIndex]);
  } else if (e == InputEvent::IdleDetected) {
    return enterIdle(next);
  }

  return next;
}

AppState enterIdle(AppState state) {
  state.uiState = UIState::Idle;
  return transition(state, InputEvent::RotateIdleData);
}

AppState transition(const AppState &state, const InputEvent e) {
  AppState next = state;

  switch (e) {
  case InputEvent::ButtonPress:
  case InputEvent::ScreenTouch:
  case InputEvent::LeftTurn:
  case InputEvent::RightTurn:
    next.lastActivityDetected = millis();
  }

  Serial.printf("Transition -> Current state: %d, Input event: %d\n",
                state.uiState, e);

  switch (state.uiState) {
  case UIState::Idle:
    return fromIdle(next, e);
  case UIState::ShowingDevices:
    return fromShowingDevices(next, e);
  case UIState::ShowingThemes:
    return fromShowingThemes(next, e);
  }

  return next;
}

void onLeftTurn() { appState = transition(appState, InputEvent::LeftTurn); }

void onRightTurn() { appState = transition(appState, InputEvent::RightTurn); }

void onButtonPressed(int pin) {
  appState = transition(appState, InputEvent::ButtonPress);
}

void onScreenTouch() {
  appState = transition(appState, InputEvent::ScreenTouch);
}

void onIdleDetected() {
  appState = transition(appState, InputEvent::IdleDetected);
}

void rotateIdleDisplay() {
  appState = transition(appState, InputEvent::RotateIdleData);
}

void showTheme(const Theme &theme) {
  screen.drawColors(theme.colorsVector, UPPER_THIRD);
  screen.writeText(theme.displayName, MIDDLE_THIRD);
}

void showDevice(const Device &device) {
  screen.writeText(device.displayName, MIDDLE_THIRD);
}

void setup() {
  Serial.begin(115200);

  delay(1000);

  AutoWiFi::State state = wifi.connect();
  if (state == AutoWiFi::State::AP_MODE) {
    screen.init([]() {});
    screen.writeText("AP Mode", XL);
    return;
  }

  client.init();

  screen.init(onScreenTouch);

  beacon.onMessage(onMessage);
  beacon.onDiscovery(onDiscovery);
  beacon.begin();

  RotaryEvents::getInstance().init(ENCODER_CLK, ENCODER_DT, onLeftTurn,
                                   onRightTurn, 2);

  button.init(BUTTON_PIN, onButtonPressed);

  rotateIdleDisplay();

  xTaskCreatePinnedToCore(udpTask, "UdpTask", 4096, nullptr, 2, nullptr, 1);
}

void loop() {
  wifi.loop();
  if (wifi.getState() == AutoWiFi::State::AP_MODE) {
    return;
  }

  screen.loop();
  beacon.loopHttp();
  button.update();

  if (shouldEnterIdle(appState.lastActivityDetected, appState.uiState)) {
    Serial.println("Idle detected");
    onIdleDetected();
  } else if (shouldRotateIdleData(appState.uiState, appState.idleData.time)) {
    Serial.println("Rotate idle data");
    rotateIdleDisplay();
  }
}
