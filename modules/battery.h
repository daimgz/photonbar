#ifndef BATTERY_H
#define BATTERY_H

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <string>
#include "module.h"
#include "../helper.h"
#include "../notifyManeger.h"

class BatteryModule : public Module {
public:
  BatteryModule() : Module("battery", false, 5) {
    iconElement.moduleName = name;
    textElement.moduleName = name;
    elements.push_back(&iconElement);
    elements.push_back(&textElement);
  }

  void update() override {
    if (readBatteryState()) {
      updateVisuals();
      checkBatteryAlert();
    }
    lastUpdate = time(nullptr);
  }

private:
  BarElement iconElement, textElement;
  long energyNow = 0, energyFull = 0, powerNow = 0;
  char status[32] = "Unknown";
  float percentage = 0.0f;
  bool notificationSent = false;

  bool readBatteryState() {
    energyNow  = readLong("energy_now", "charge_now");
    energyFull = readLong("energy_full", "charge_full");
    powerNow   = readLong("power_now", "current_now");

    FILE* f = fopen("/sys/class/power_supply/BAT0/status", "r");
    if (f) {
      if (fscanf(f, "%31s", status) != 1) strcpy(status, "Unknown");
      fclose(f);
    }

    if (energyFull > 0) {
      percentage = ((float)energyNow / (float)energyFull) * 100.0f;
      return true;
    }
    return false;
  }

  void updateVisuals() {
    bool isCharging = (strcmp(status, "Charging") == 0);

    // 1. Contenido
    snprintf(iconElement.content, CONTENT_MAX_LEN, "%s ", Helper::getBatteryIcon(percentage, isCharging));

    if (powerNow > 0 && (isCharging || strcmp(status, "Discharging") == 0)) {
      float timeFloat = isCharging ? (float)(energyFull - energyNow) / powerNow
        : (float)energyNow / powerNow;
      int h = (int)timeFloat;
      int m = (int)((timeFloat - h) * 60);
      snprintf(textElement.content, CONTENT_MAX_LEN, "%.1f%% %02d:%02d", percentage, h, m);
    } else {
      snprintf(textElement.content, CONTENT_MAX_LEN, "%.1f%%", percentage);
    }

    // 2. Colores (Respetando tus llamadas originales a parse_color)
    // Texto: Púrpura por defecto, Rojo si es crítico y no carga
    if (percentage < 20.0f && !isCharging) {
      textElement.foregroundColor = Color::parse_color("#FF0000", NULL, Color(255, 0, 0, 255));
    } else {
      textElement.foregroundColor = Color::parse_color("#E0AAFF", NULL, Color(224, 170, 255, 255));
    }

    // Icono: Lógica de carga original con stringstream eliminada para ganar velocidad
    if (isCharging) {
      if (percentage >= 90.0f) {
        // Verde
        iconElement.foregroundColor = Color::parse_color("#00FF00", NULL, Color(0, 255, 0, 255));
      } else if (percentage >= 20.0f) {
        // Naranja (255, 165, 0) -> #FFA500
        iconElement.foregroundColor = Color::parse_color("#FFA500", NULL, Color(255, 165, 0, 255));
      } else {
        // Rojo
        iconElement.foregroundColor = Color::parse_color("#FF0000", NULL, Color(255, 0, 0, 255));
      }
    } else {
      // No carga -> Púrpura
      iconElement.foregroundColor = Color::parse_color("#E0AAFF", NULL, Color(224, 170, 255, 255));
    }

    iconElement.dirtyContent = true;
    textElement.dirtyContent = true;
  }

  void checkBatteryAlert() {
    if (percentage < 20.0f && strcmp(status, "Charging") != 0 && !notificationSent) {
      std::string msg = "Nivel actual: <b>" + std::to_string((int)percentage) + "%</b>\n<b>Conecta el cargador de inmediato.</b>";
      NotifyManager::instance().send("󰂃 Batería Crítica", msg, NOTIFY_URGENCY_CRITICAL);
      notificationSent = true;
    } else if (percentage > 25.0f || strcmp(status, "Charging") == 0) {
      notificationSent = false;
    }
  }

  long readLong(const char* f1, const char* f2) {
    char path[64];
    long val = 0;
    snprintf(path, sizeof(path), "/sys/class/power_supply/BAT0/%s", f1);
    FILE* f = fopen(path, "r");
    if (!f && f2) {
      snprintf(path, sizeof(path), "/sys/class/power_supply/BAT0/%s", f2);
      f = fopen(path, "r");
    }
    if (f) {
      fscanf(f, "%ld", &val);
      fclose(f);
    }
    return val;
  }
};

#endif
