#ifndef BATTERY_H
#define BATTERY_H

#include <stdio.h>
#include <string.h>
#include <time.h>
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
    // Cacheamos los valores de los archivos de sistema
    energyNow  = readLong("energy_now", "charge_now");
    energyFull = readLong("energy_full", "charge_full");
    powerNow   = readLong("power_now", "current_now");

    // Leemos el estado una sola vez
    readStatus();

    if (energyFull > 0) {
      percentage = ((float)energyNow / (float)energyFull) * 100.0f;
      updateVisuals();
      checkBatteryAlert();
    }

    lastUpdate = time(nullptr);
  }

private:
  BarElement iconElement, textElement;
  long energyNow = 0, energyFull = 0, powerNow = 0;
  char status[16] = "Unknown";
  float percentage = 0.0f;
  bool notificationSent = false;

  // Optimización: Solo comparamos el primer carácter para ganar velocidad
  // 'C' = Charging, 'D' = Discharging, 'F' = Full
  void updateVisuals() {
    const bool isCharging = (status[0] == 'C');

    // 1. Icono y Texto
    snprintf(iconElement.content, CONTENT_MAX_LEN, "%s ", Helper::getBatteryIcon(percentage, isCharging));

    if (powerNow > 0 && (isCharging || status[0] == 'D')) {
      float timeFloat = isCharging ? (float)(energyFull - energyNow) / powerNow
        : (float)energyNow / powerNow;
      int totalMins = (int)(timeFloat * 60);
      snprintf(textElement.content, CONTENT_MAX_LEN, "%.1f%% %02d:%02d", percentage, totalMins / 60, totalMins % 60);
    } else {
      snprintf(textElement.content, CONTENT_MAX_LEN, "%.1f%%", percentage);
    }

    // 2. Colores (Estética original preservada)
    if (isCharging) {
      if (percentage >= 90.0f)
        iconElement.foregroundColor = Color::parse_color("#00FF00", NULL, Color(0, 255, 0, 255));
      else if (percentage >= 20.0f)
        iconElement.foregroundColor = Color::parse_color("#FFA500", NULL, Color(255, 165, 0, 255));
      else
        iconElement.foregroundColor = Color::parse_color("#FF0000", NULL, Color(255, 0, 0, 255));
    } else {
      iconElement.foregroundColor = Color::parse_color("#E0AAFF", NULL, Color(224, 170, 255, 255));
    }

    // Texto: Rojo solo si es crítico y no carga
    if (percentage < 20.0f && !isCharging) {
      textElement.foregroundColor = Color::parse_color("#FF0000", NULL, Color(255, 0, 0, 255));
    } else {
      textElement.foregroundColor = Color::parse_color("#E0AAFF", NULL, Color(224, 170, 255, 255));
    }

    iconElement.dirtyContent = true;
    textElement.dirtyContent = true;
  }

  void checkBatteryAlert() {
    const bool isCharging = (status[0] == 'C');

    if (percentage < 20.0f && !isCharging && !notificationSent) {
      char msg[128];
      snprintf(msg, sizeof(msg), "Nivel actual: <b>%d%%</b>\n<b>Conecta el cargador de inmediato.</b>", (int)percentage);
      NotifyManager::instance().send("󰂃 Batería Crítica", msg, NOTIFY_URGENCY_CRITICAL);
      notificationSent = true;
    } else if (percentage > 25.0f || isCharging) {
      notificationSent = false;
    }
  }

  // Funciones de utilidad ligeras
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
      if (fscanf(f, "%ld", &val) != 1) val = 0;
      fclose(f);
    }
    return val;
  }

  void readStatus() {
    FILE* f = fopen("/sys/class/power_supply/BAT0/status", "r");
    if (f) {
      if (fscanf(f, "%15s", status) != 1) strcpy(status, "Unknown");
      fclose(f);
    }
  }
};

#endif
