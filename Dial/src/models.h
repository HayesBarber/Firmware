#pragma once

#include <Arduino.h>
#include <vector>

struct Theme {
  String displayName;
  std::vector<uint16_t> colorsVector;
  String applyUrl;
};

struct Device {
  String displayName;
  String toggleUrl;
};
