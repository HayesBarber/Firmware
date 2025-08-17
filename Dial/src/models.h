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

struct AppState {
  std::vector<Device> devices;
  std::vector<Theme> themes;
  IdleData idleData;
  unsigned long lastActivityDetected;
  bool isNight;
  int rotationIndex;
  UIState uiState;
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
