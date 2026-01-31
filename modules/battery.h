#ifndef BATTERY_H
#define BATTERY_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sstream>
#include <iomanip>
#include "module.h"
#include "../helper.h"
#include "../notifyManeger.h"

class BatteryModule : public Module {
public:
  BatteryModule() : Module("battery", false, 5) {
    iconElement.moduleName = name;
    elements.push_back(&iconElement);

    textElement.moduleName = name;
    elements.push_back(&textElement);
  }

  void update() override {
    updateBatteryInfo();
    checkBatteryAlert(); // Nueva función de chequeo
  }

private:
  BarElement iconElement;
  BarElement textElement;
  long energyNow = 0, energyFull = 0, powerNow = 0;
  char status[32] = "Unknown";
  float percentage = 0.0;
  bool notificationSent = false; // Flag para no spamear notificaciones

  void checkBatteryAlert() {
    // Si la batería es < 20%, no está cargando y no hemos enviado notificación aún
    if (
      (
        percentage < 20.0f ||
        percentage < 15.0f ||
        percentage < 10.0f ||
        percentage < 5.0f
      ) && strcmp(status, "Charging") != 0 && !notificationSent
    ) {

      NotifyManager::instance().send(
        "󰂃 Batería Crítica",
        "Nivel actual: <b>" + std::to_string((int)percentage) + "%</b>\n<b>Conecta el cargador de inmediato.</b>",
        NOTIFY_URGENCY_CRITICAL
      );
      notificationSent = true;
    }

    // Resetear el flag si la batería sube del 25% o se pone a cargar
    // (Usamos 25% para evitar que la notificación salte repetidamente si oscila en 20%)
    if (percentage > 25.0f || strcmp(status, "Charging") == 0) {
      notificationSent = false;
    }
  }

  void updateBatteryInfo() {
    energyNow = energyFull = powerNow = 0;
    strcpy(status, "Unknown");
    percentage = 0.0;

    FILE *f_now = fopen("/sys/class/power_supply/BAT0/energy_now", "r");
    if (!f_now) f_now = fopen("/sys/class/power_supply/BAT0/charge_now", "r");

    FILE *f_full = fopen("/sys/class/power_supply/BAT0/energy_full", "r");
    if (!f_full) f_full = fopen("/sys/class/power_supply/BAT0/charge_full", "r");

    FILE *f_pow = fopen("/sys/class/power_supply/BAT0/power_now", "r");
    if (!f_pow) f_pow = fopen("/sys/class/power_supply/BAT0/current_now", "r");

    FILE *f_stat = fopen("/sys/class/power_supply/BAT0/status", "r");

    if (f_now && f_full) {
      fscanf(f_now, "%ld", &energyNow);
      fscanf(f_full, "%ld", &energyFull);
      if (energyFull > 0) percentage = ((float)energyNow / (float)energyFull) * 100.0;
      fclose(f_now); fclose(f_full);
    }
    if (f_pow) { fscanf(f_pow, "%ld", &powerNow); fclose(f_pow); }
    if (f_stat) { fscanf(f_stat, "%s", status); fclose(f_stat); }

    const char* icon = Helper::getBatteryIcon(percentage, strcmp(status, "Charging") == 0);

    // Actualización de elementos visuales (acortado para brevedad)
    if (powerNow > 0 && (strcmp(status, "Discharging") == 0 || strcmp(status, "Charging") == 0)) {
      float timeFloat = (strcmp(status, "Discharging") == 0) ? (float)energyNow / powerNow : (float)(energyFull - energyNow) / powerNow;
      int hours = (int)timeFloat;
      int minutes = (int)((timeFloat - hours) * 60);

      snprintf(iconElement.content, CONTENT_MAX_LEN, "%s ", icon);
      snprintf(textElement.content, CONTENT_MAX_LEN, "%.1f%% %02d:%02d", percentage, hours, minutes);
    } else {
      snprintf(iconElement.content, CONTENT_MAX_LEN, "%s", icon);
      snprintf(textElement.content, CONTENT_MAX_LEN, "%.1f%%", percentage);
    }

    iconElement.dirtyContent = true;
    textElement.dirtyContent = true;
    updateColors();
    lastUpdate = time(nullptr);
  }

  void updateColors() {
    textElement.foregroundColor = Color::parse_color("#E0AAFF", NULL, Color(224, 170, 255, 255));

    if (strcmp(status, "Charging") == 0) {
      int r = (percentage >= 90) ? 0 : 255;
      int g = (percentage >= 90) ? 255 : (percentage >= 20 ? 165 : 0);
      int b = 0;
      std::stringstream ss;
      ss << "#" << std::hex << std::setfill('0') << std::setw(2) << r << std::setw(2) << g << std::setw(2) << b;
      iconElement.foregroundColor = Color::parse_color(ss.str().c_str(), NULL, Color(r, g, b, 255));
    } else {
      iconElement.foregroundColor = Color::parse_color("#E0AAFF", NULL, Color(224, 170, 255, 255));
    }

    if (percentage < 20 && strcmp(status, "Charging") != 0) {
      textElement.foregroundColor = Color::parse_color("#FF0000", NULL, Color(255, 0, 0, 255));
    }
  }
};

#endif
