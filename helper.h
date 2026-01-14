#pragma once
#include <cstdint>

namespace Helper {
  static constexpr const char* ICON_BATTERY_CHARGING_0   = u8"\U000f089f";
  static constexpr const char* ICON_BATTERY_CHARGING_10  = u8"\U000f089c";
  static constexpr const char* ICON_BATTERY_CHARGING_20  = u8"\U000f0086";
  static constexpr const char* ICON_BATTERY_CHARGING_30  = u8"\U000f0087";
  static constexpr const char* ICON_BATTERY_CHARGING_40  = u8"\U000f0088";
  static constexpr const char* ICON_BATTERY_CHARGING_50  = u8"\U000f089d";
  static constexpr const char* ICON_BATTERY_CHARGING_60  = u8"\U000f0089";
  static constexpr const char* ICON_BATTERY_CHARGING_70  = u8"\U000f089e";
  static constexpr const char* ICON_BATTERY_CHARGING_80  = u8"\U000f008a";
  static constexpr const char* ICON_BATTERY_CHARGING_90  = u8"\U000f008b";
  static constexpr const char* ICON_BATTERY_CHARGING_100 = u8"\U000f0085";

  static constexpr const char* ICON_BATTERY_0            = u8"\U000f008e";
  static constexpr const char* ICON_BATTERY_10           = u8"\U000f007a";
  static constexpr const char* ICON_BATTERY_20           = u8"\U000f007b";
  static constexpr const char* ICON_BATTERY_30           = u8"\U000f007c";
  static constexpr const char* ICON_BATTERY_40           = u8"\U000f007d";
  static constexpr const char* ICON_BATTERY_50           = u8"\U000f007e";
  static constexpr const char* ICON_BATTERY_60           = u8"\U000f007f";
  static constexpr const char* ICON_BATTERY_70           = u8"\U000f0080";
  static constexpr const char* ICON_BATTERY_80           = u8"\U000f0081";
  static constexpr const char* ICON_BATTERY_90           = u8"\U000f0082";
  static constexpr const char* ICON_BATTERY_100          = u8"\U000f0079";

  inline const char* getBatteryIcon(const uint8_t percentage, const bool charging = false) {
    if (charging) {
      if (percentage >= 95) return ICON_BATTERY_CHARGING_100;
      else if (percentage >= 90) return ICON_BATTERY_CHARGING_90;
      else if (percentage >= 80) return ICON_BATTERY_CHARGING_80;
      else if (percentage >= 70) return ICON_BATTERY_CHARGING_70;
      else if (percentage >= 60) return ICON_BATTERY_CHARGING_60;
      else if (percentage >= 50) return ICON_BATTERY_CHARGING_50;
      else if (percentage >= 40) return ICON_BATTERY_CHARGING_40;
      else if (percentage >= 30) return ICON_BATTERY_CHARGING_30;
      else if (percentage >= 20) return ICON_BATTERY_CHARGING_20;
      else if (percentage >= 10) return ICON_BATTERY_CHARGING_10;
      else return ICON_BATTERY_CHARGING_0;

    } else {
      if (percentage >= 95) return ICON_BATTERY_100;
      else if (percentage >= 90) return ICON_BATTERY_90;
      else if (percentage >= 80) return ICON_BATTERY_80;
      else if (percentage >= 70) return ICON_BATTERY_70;
      else if (percentage >= 60) return ICON_BATTERY_60;
      else if (percentage >= 50) return ICON_BATTERY_50;
      else if (percentage >= 40) return ICON_BATTERY_40;
      else if (percentage >= 30) return ICON_BATTERY_30;
      else if (percentage >= 20) return ICON_BATTERY_20;
      else if (percentage >= 10) return ICON_BATTERY_10;
      else return ICON_BATTERY_0;

    }
  }
}
