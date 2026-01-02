#ifndef BATTERY_H
#define BATTERY_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "module.h"

class BatteryModule : public Module {
  public:
    BatteryModule():
      Module("battery")
    {
      // Battery solo necesita actualizarse cada 30 segundos
      setUpdatePerIteration(false);
      setSecondsPerUpdate(3);
    }

    void update() {
      long energy_now = 0, energy_full = 0, power_now = 0;
      char status[32] = "Unknown";
      float percentage = 0.0;
      int hours, minutes;

      // Iconos UTF-8 de Nerd Fonts para batería
      static constexpr const char* ICON_BOLT = u8"\U000f0079";         //  Rayo (cargando)

      static constexpr const char* ICON_BATTERY_CHARGING_0 = u8"\U000f089f";  //  Batería 25%
      static constexpr const char* ICON_BATTERY_CHARGING_10 = u8"\U000f089c";  //  Batería 25%
      static constexpr const char* ICON_BATTERY_CHARGING_20 = u8"\U000f0086";  //  Batería 25%
      static constexpr const char* ICON_BATTERY_CHARGING_30 = u8"\U000f0087";  //  Batería 25%
      static constexpr const char* ICON_BATTERY_CHARGING_40 = u8"\U000f0088";  //  Batería 25%
      static constexpr const char* ICON_BATTERY_CHARGING_50 = u8"\U000f089d";  //  Batería 25%
      static constexpr const char* ICON_BATTERY_CHARGING_60 = u8"\U000f0089";  //  Batería 25%
      static constexpr const char* ICON_BATTERY_CHARGING_70 = u8"\U000f089e";  //  Batería 25%
      static constexpr const char* ICON_BATTERY_CHARGING_80 = u8"\U000f008a";  //  Batería 25%
      static constexpr const char* ICON_BATTERY_CHARGING_90 = u8"\U000f008b";  //  Batería 25%
      static constexpr const char* ICON_BATTERY_CHARGING_100 = u8"\U000f0085";  //  Batería 25%
    //
      static constexpr const char* ICON_BATTERY_0 = u8"\U000f008e";  //  Batería 25%
      static constexpr const char* ICON_BATTERY_10 = u8"\U000f007a";  //  Batería 25%
      static constexpr const char* ICON_BATTERY_20 = u8"\U000f007b";  //  Batería 25%
      static constexpr const char* ICON_BATTERY_30 = u8"\U000f007c";  //  Batería 25%
      static constexpr const char* ICON_BATTERY_40 = u8"\U000f007d";  //  Batería 25%
      static constexpr const char* ICON_BATTERY_50 = u8"\U000f007e";  //  Batería 25%
      static constexpr const char* ICON_BATTERY_60 = u8"\U000f007f";  //  Batería 25%
      static constexpr const char* ICON_BATTERY_70 = u8"\U000f0080";  //  Batería 25%
      static constexpr const char* ICON_BATTERY_80 = u8"\U000f0081";  //  Batería 25%
      static constexpr const char* ICON_BATTERY_90 = u8"\U000f0082";  //  Batería 25%
      static constexpr const char* ICON_BATTERY_100 = u8"\U000f0079";  //  Batería 25%

      static constexpr const char* ICON_BATTERY_EMPTY = "\xef\x89\x84"; //  Batería 0% (crítico)

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

          static char icon_color_buffer[32];
          sprintf(icon_color_buffer, "#%02X%02X%02X", r, g, b);
          icon_color = icon_color_buffer;
      }

      char temp_buffer[256];
      if (power_now > 0 && (strcmp(status, "Discharging") == 0 || strcmp(status, "Charging") == 0)) {
          float time_float;
          if (strcmp(status, "Discharging") == 0)
              time_float = (float)energy_now / power_now;
          else
              time_float = (float)(energy_full - energy_now) / power_now;

          hours = (int)time_float;
          minutes = (int)((time_float - hours) * 60);

          if (strcmp(status, "Charging") == 0) {
              sprintf(temp_buffer, "%%{F%s}%s%%{F-} %%{F%s}%.2f%% %02d:%02d%%{F-}", icon_color, icon, color, percentage, hours, minutes);
          } else {
              sprintf(temp_buffer, "%%{F%s}%s %.2f%% %02d:%02d%%{F-}", color, icon, percentage, hours, minutes);
          }
      } else {
          if (strcmp(status, "Charging") == 0) {
              sprintf(temp_buffer, "%%{F%s}%s%%{F-} %%{F%s}%.2f%%%%{F-}", icon_color, icon, color, percentage);
          } else {
              sprintf(temp_buffer, "%%{F%s}%s %.2f%%{F-}", color, icon, percentage);
          }
      }

      buffer = temp_buffer;
    }

    void event(const char* eventValue) {
    }

  private:
};

#endif // BATTERY_H
