#ifndef BATTERY_H
#define BATTERY_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sstream>
#include <iomanip>
#include "module.h"

class BatteryModule : public Module {
  public:
    // Iconos UTF-8 de Nerd Fonts para batería
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

    BatteryModule():
      Module("battery")
    {
      // Battery solo necesita actualizarse cada 5 segundos
      setUpdatePerIteration(false);
      setSecondsPerUpdate(5);
    }

    void update() {
      long energy_now = 0, energy_full = 0, power_now = 0;
      char status[32] = "Unknown";
      float percentage = 0.0;
      int hours, minutes;

      const char* icon;

      FILE *f_now = fopen("/sys/class/power_supply/BAT0/energy_now", "r");
      if (!f_now) f_now = fopen("/sys/class/power_supply/BAT0/charge_now", "r");

      FILE *f_full = fopen("/sys/class/power_supply/BAT0/energy_full", "r");
      if (!f_full) f_full = fopen("/sys/class/power_supply/BAT0/charge_full", "r");

      FILE *f_pow = fopen("/sys/class/power_supply/BAT0/power_now", "r");
      if (!f_pow) f_pow = fopen("/sys/class/power_supply/BAT0/current_now", "r");

      FILE *f_stat = fopen("/sys/class/power_supply/BAT0/status", "r");

      if (f_now && f_full) {
          fscanf(f_now, "%ld", &energy_now);
          fscanf(f_full, "%ld", &energy_full);
          if (energy_full > 0) percentage = ((float)energy_now / (float)energy_full) * 100.0;
          fclose(f_now); fclose(f_full);
      }
      if (f_pow) { fscanf(f_pow, "%ld", &power_now); fclose(f_pow); }
      if (f_stat) { fscanf(f_stat, "%s", status); fclose(f_stat); }

      if (strcmp(status, "Charging") == 0) {
          if (percentage >= 95) icon = ICON_BATTERY_CHARGING_100;
          else if (percentage >= 90) icon = ICON_BATTERY_CHARGING_90;
          else if (percentage >= 80) icon = ICON_BATTERY_CHARGING_80;
          else if (percentage >= 70) icon = ICON_BATTERY_CHARGING_70;
          else if (percentage >= 60) icon = ICON_BATTERY_CHARGING_60;
          else if (percentage >= 50) icon = ICON_BATTERY_CHARGING_50;
          else if (percentage >= 40) icon = ICON_BATTERY_CHARGING_40;
          else if (percentage >= 30) icon = ICON_BATTERY_CHARGING_30;
          else if (percentage >= 20) icon = ICON_BATTERY_CHARGING_20;
          else if (percentage >= 10) icon = ICON_BATTERY_CHARGING_10;
          else                       icon = ICON_BATTERY_CHARGING_0;
      } else {
          if (percentage >= 95) icon = ICON_BATTERY_100;
          else if (percentage >= 90) icon = ICON_BATTERY_90;
          else if (percentage >= 80) icon = ICON_BATTERY_80;
          else if (percentage >= 70) icon = ICON_BATTERY_70;
          else if (percentage >= 60) icon = ICON_BATTERY_60;
          else if (percentage >= 50) icon = ICON_BATTERY_50;
          else if (percentage >= 40) icon = ICON_BATTERY_40;
          else if (percentage >= 30) icon = ICON_BATTERY_30;
          else if (percentage >= 20) icon = ICON_BATTERY_20;
          else if (percentage >= 10) icon = ICON_BATTERY_10;
          else                       icon = ICON_BATTERY_0;
      }

      const char *color = (percentage < 20 && strcmp(status, "Charging") != 0) ? Module::COLOR_ALERT : Module::COLOR_FG;

      // Calcular color para el rayito si está cargando
      const char* icon_color = color;
      if (strcmp(status, "Charging") == 0 && percentage > 0) {
          int r, g, b;

          if (percentage >= 90) {
              // Verde puro al 90% o más
              r = 0; g = 255; b = 0;
          } else {
              // Interpolar entre rojo (255,0,0) y naranja (255,165,0) hasta el 90%
              float ratio = percentage / 90.0f;
              r = 255;
              g = (int)(165 * ratio);
              b = 0;
          }

          std::stringstream ss;
          ss << "#" << std::hex << std::setw(2) << std::setfill('0') << r
             << std::setw(2) << std::setfill('0') << g
             << std::setw(2) << std::setfill('0') << b;
          static std::string icon_color_str = ss.str();
          icon_color = icon_color_str.c_str();
      }

      if (power_now > 0 && (strcmp(status, "Discharging") == 0 || strcmp(status, "Charging") == 0)) {
          float time_float;
          if (strcmp(status, "Discharging") == 0)
              time_float = (float)energy_now / power_now;
          else
              time_float = (float)(energy_full - energy_now) / power_now;

          hours = (int)time_float;
          minutes = (int)((time_float - hours) * 60);

          std::stringstream ss;
          if (strcmp(status, "Charging") == 0) {
              ss << "%{F" << icon_color << "}" << icon << "%{F-} %{F" << color << "}"
                 << std::fixed << std::setprecision(2) << percentage << "% "
                 << std::setw(2) << std::setfill('0') << hours << ":"
                 << std::setw(2) << std::setfill('0') << minutes << "%{F-}";
          } else {
              ss << "%{F" << color << "}" << icon << " "
                 << std::fixed << std::setprecision(2) << percentage << "% "
                 << std::setw(2) << std::setfill('0') << hours << ":"
                 << std::setw(2) << std::setfill('0') << minutes << "%{F-}";
          }
          buffer = ss.str();
      } else {
          std::stringstream ss;
          if (strcmp(status, "Charging") == 0) {
              ss << "%{F" << icon_color << "}" << icon << "%{F-} %{F" << color << "}"
                 << std::fixed << std::setprecision(2) << percentage << "%{F-}";
          } else {
              ss << "%{F" << color << "}" << icon << " "
                 << std::fixed << std::setprecision(2) << percentage << "%{F-}";
          }
          buffer = ss.str();
      }
    }

    void event(const char* eventValue) {
    }

  private:
};

#endif // BATTERY_H
