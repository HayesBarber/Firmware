#pragma once

#include <Arduino.h>
#include <Message.h>
#include <vector>

struct Theme {
  String displayName;
  String colors;
  std::vector<uint16_t> colorsVector;

  static Theme parseTheme(const String name, const String colors) {
    Theme t;
    t.displayName = name;
    t.colors = colors;
    // todo set colors vector
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

  static AppState fromCheckinResponse(const Message &msg,
                                      const AppState &currState) {
    auto device_names = msg.getArrayProperty("device_names");
    auto theme_names = msg.getArrayProperty("theme_names");
    auto theme_color_strings = msg.getArrayProperty("theme_colors");
    auto extras = msg.getArrayProperty("extras");

    AppState newState = currState;

    newState.devices.clear();
    for (const auto &name : device_names) {
      newState.devices.push_back(Device::parseDevice(name));
    }
    if (newState.devices.empty()) {
      newState.devices.push_back(Device{"No Devices", ""});
    }

    newState.themes.clear();
    for (size_t i = 0; i < theme_names.size(); i++) {
      bool hasCompliment = i < theme_color_strings.size();
      if (!hasCompliment) {
        continue;
      }
      String colors = theme_color_strings[i];
      newState.themes.push_back(Theme::parseTheme(theme_names[i], colors));
    }
    if (newState.themes.empty()) {
      newState.themes.push_back(Theme{"No Themes", "", {}});
    }

    newState.idleData.index = 0;
    newState.idleData.time = msg.getProperty("epoch_time_seconds");
    newState.idleData.extras = extras;

    return newState;
  }
};
