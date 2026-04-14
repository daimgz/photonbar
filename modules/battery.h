#ifndef BATTERY_H
#define BATTERY_H

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <chrono>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include "module.h"
#include "../helper.h"
#include "../notifyManeger.h"
#include "../color_cache.h"

class BatteryModule : public Module {
public:
  BatteryModule() : Module("battery", false, 5), upowerFd(-1), upowerPid(-1) {
    iconElement.moduleName = name;
    textElement.moduleName = name;
    elements.push_back(&iconElement);
    elements.push_back(&textElement);
  }

  ~BatteryModule() {
    cleanup();
  }

  bool initialize() override {
    return initializeCommand();
  }

  int setupSelectFds(fd_set& fds) override {
    if (upowerFd < 0) return -1;
    FD_SET(upowerFd, &fds);
    return upowerFd;
  }

  bool handleEvents(fd_set& fds) override {
    static auto lastEventUpdate = std::chrono::steady_clock::time_point{};
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastEventUpdate).count() < 250) {
      return false;
    }

    if (upowerFd < 0 || !FD_ISSET(upowerFd, &fds)) return false;

    char buffer[4096];
    ssize_t n = read(upowerFd, buffer, sizeof(buffer) - 1);

    if (n <= 0) {
      initializeCommand();
      return false;
    }

    buffer[n] = '\0';

    if (strstr(buffer, "battery_BAT0")) {
      lastEventUpdate = std::chrono::steady_clock::now();
      return true;
    }

    return false;
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
  int upowerFd;
  pid_t upowerPid;
  BarElement iconElement, textElement;
  long energyNow = 0, energyFull = 0, powerNow = 0;
  char status[16] = "Unknown";
  float percentage = 0.0f;
  bool notificationSent = false;

  bool initializeCommand() {
    cleanup();

    int pipefd[2];
    if (pipe(pipefd) == -1) return false;

    upowerPid = fork();
    if (upowerPid == -1) {
      close(pipefd[0]);
      close(pipefd[1]);
      return false;
    }

    if (upowerPid == 0) {
      close(pipefd[0]);
      dup2(pipefd[1], STDOUT_FILENO);
      close(pipefd[1]);

      execlp("upower", "upower", "-m", NULL);
      _exit(1);
    }

    close(pipefd[1]);
    upowerFd = pipefd[0];

    int flags = fcntl(upowerFd, F_GETFL, 0);
    fcntl(upowerFd, F_SETFL, flags | O_NONBLOCK);

    return true;
  }

  void cleanup() {
    if (upowerFd >= 0) {
      close(upowerFd);
      upowerFd = -1;
    }
    if (upowerPid > 0) {
      kill(upowerPid, SIGTERM);
      waitpid(upowerPid, NULL, 0);
      upowerPid = -1;
    }
  }

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
        iconElement.foregroundColor = ColorCache::green();
      else if (percentage >= 20.0f)
        iconElement.foregroundColor = ColorCache::orange();
      else
        iconElement.foregroundColor = ColorCache::red();
    } else {
      iconElement.foregroundColor = ColorCache::purple();
    }

    // Texto: Rojo solo si es crítico y no carga
    if (percentage < 20.0f && !isCharging) {
      textElement.foregroundColor = ColorCache::red();
    } else {
      textElement.foregroundColor = ColorCache::purple();
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