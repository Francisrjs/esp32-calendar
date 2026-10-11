#pragma once
#include <Arduino.h>
#include "power_rules.h"
struct PowerConfig {
  uint32_t revision=0;
  uint16_t idleSeconds=60, wifiMinutes=15, absenceCheckMinutes=0;
  uint8_t profile=0; // 0 available, 1 balanced, 2 maximum
  bool micOff=false, intervalWifi=false;
  uint8_t sleepMode=0; // none/light/deep
  bool wakeAlarm=true;
  AbsenceRule rules[8];
  uint8_t count=0;
};
bool parsePowerConfig(const String& json,PowerConfig& output,String& error);
String encodePowerConfig(const PowerConfig& config);
