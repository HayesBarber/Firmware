#pragma once

#include <Arduino.h>
#include <vector>

struct Theme {
  String displayName;
  String colors;
  std::vector<uint16_t> colorsVector;

  static Theme parseTheme(const String name, const String colors) {
    Theme t;
    t.displayName = name;
    t.colors = colors;
    return t;
  }
};

struct Device {
  String displayName;
  String toggleUrl;

  static Device parseDevice(const String name) {
    return Device{name, "/lighting/" + name + "/toggle"};
  }
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
