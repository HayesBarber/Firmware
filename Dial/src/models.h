#pragma once

#include <Arduino.h>
#include <vector>

struct Theme {
  String displayName;
  String colors;
  std::vector<uint16_t> colorsVector;
};

struct Device {
  String displayName;
  String toggleUrl;
};

struct IdleData {
  int index;
  String time;
  std::vector<String> extras;
};

enum class UIState { Idle, ShowingDevices, ShowingThemes };

enum class InputEvent {
  LeftTurn,
  RightTurn,
  ButtonPress,
  ScreenTouch,
  IdleDetected,
  RotateIdleData
};

struct AppState {
  std::vector<Device> devices;
  std::vector<Theme> themes;
  IdleData idleData;
  unsigned long lastActivityDetected;
  int rotationIndex;
  UIState uiState;

  static AppState makeInitialAppState() {
    AppState s;
    s.lastActivityDetected = millis();
    s.rotationIndex = 0;
    s.uiState = UIState::Idle;
    s.idleData.index = 0;
    s.idleData.time = "? PM";
    s.devices = {Device{"No Devices", ""}};
    s.themes = {Theme{"No Themes", "", {}}};
    return s;
  }
};
