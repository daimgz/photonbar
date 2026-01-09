#ifndef RESOURCES_H
#define RESOURCES_H

#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "module.h"
#include "../barElement.h"
#include "../color.h"

class ResourcesModule : public Module {
  private:
    // ================= CPU STATE =================
    uint64_t lastCpuTotal = 0;
    uint64_t lastCpuIdle  = 0;

    // ================= TEMP STATE =================
    int tempFds[32];
    int tempFdCount = 0;
    bool tempInitialized = false;

    // ================= UI ELEMENTS =================
    BarElement ramElement;
    BarElement cpuElement;
    BarElement tempElement;

    // ================= COLORS =================
    Color colorNormal;
    Color colorAlert;

    // ================= ICONS =================
    static constexpr const char* ICON_RAM  = "\uefc5";
    static constexpr const char* ICON_CPU  = "\uf4bc";
    static constexpr const char* ICON_TEMP = "\uf2c9";

    // ================= THRESHOLDS =================
    static constexpr float RAM_WARN  = 80.0f;
    static constexpr float CPU_WARN  = 80.0f;
    static constexpr float TEMP_WARN = 70.0f;

  public:
    ResourcesModule() : Module("resources", false, 2) {
      // ---- cachear colores (MUY importante) ----
      colorNormal = Color::parse_color("#E0AAFF", nullptr, Color(224,170,255,255));
      colorAlert  = Color::parse_color("#FF6B6B", nullptr, Color(255,107,107,255));

      // ---- RAM ----
      ramElement.moduleName = name;
      ramElement.foregroundColor = colorNormal;
      elements.push_back(&ramElement);

      // ---- CPU ----
      cpuElement.moduleName = name;
      cpuElement.foregroundColor = colorNormal;
      elements.push_back(&cpuElement);

      // ---- TEMP ----
      tempElement.moduleName = name;
      tempElement.foregroundColor = colorNormal;
      elements.push_back(&tempElement);
    }

    ~ResourcesModule() {
      for (int i = 0; i < tempFdCount; ++i)
        close(tempFds[i]);
    }

    // ================= RAM =================
    float getRamUsage() {
      FILE* f = fopen("/proc/meminfo", "r");
      if (!f) return 0.0f;

      char line[256];
      uint64_t total = 0, avail = 0;

      while (fgets(line, sizeof(line), f)) {
        if (!total && !strncmp(line, "MemTotal:", 9))
          total = strtoull(line + 9, nullptr, 10);
        else if (!avail && !strncmp(line, "MemAvailable:", 13))
          avail = strtoull(line + 13, nullptr, 10);
        if (total && avail) break;
      }
      fclose(f);

      if (!total) return 0.0f;
      return 100.0f - (avail * 100.0f) / total;
    }

    // ================= CPU =================
    float getCpuUsage() {
      int fd = open("/proc/stat", O_RDONLY | O_CLOEXEC);
      if (fd < 0) return 0.0f;

      char buf[256];
      ssize_t n = read(fd, buf, sizeof(buf) - 1);
      close(fd);
      if (n <= 0) return 0.0f;
      buf[n] = '\0';

      const char* p = buf + 4; // "cpu "

      uint64_t v[8];
      for (int i = 0; i < 8; ++i)
        v[i] = strtoull(p, (char**)&p, 10);

      uint64_t idle  = v[3] + v[4];
      uint64_t total = v[0] + v[1] + v[2] + idle + v[5] + v[6] + v[7];

      if (!lastCpuTotal) {
        lastCpuTotal = total;
        lastCpuIdle  = idle;
        return 0.0f;
      }

      uint64_t diffTotal = total - lastCpuTotal;
      uint64_t diffIdle  = idle  - lastCpuIdle;

      lastCpuTotal = total;
      lastCpuIdle  = idle;

      if (!diffTotal) return 0.0f;
      return 100.0f * (1.0f - (float)diffIdle / diffTotal);
    }

    // ================= TEMP =================
    float getCpuTemp() {
      if (!tempInitialized) {
        DIR* dir = opendir("/sys/class/hwmon");
        if (dir) {
          struct dirent* ent;
          while ((ent = readdir(dir)) && tempFdCount < 32) {
            if (ent->d_name[0] == '.') continue;

            char base[128];
            int blen = 0;
            const char* p = "/sys/class/hwmon/";
            while (*p) base[blen++] = *p++;
            p = ent->d_name;
            while (*p) base[blen++] = *p++;
            base[blen] = '\0';

            DIR* sub = opendir(base);
            if (!sub) continue;

            struct dirent* e2;
            while ((e2 = readdir(sub)) && tempFdCount < 32) {
              if (strncmp(e2->d_name, "temp", 4) != 0) continue;
              if (!strstr(e2->d_name, "_input")) continue;

              char file[256];
              int flen = 0;
              p = base;
              while (*p) file[flen++] = *p++;
              file[flen++] = '/';
              p = e2->d_name;
              while (*p) file[flen++] = *p++;
              file[flen] = '\0';

              int fd = open(file, O_RDONLY | O_CLOEXEC);
              if (fd >= 0)
                tempFds[tempFdCount++] = fd;
            }
            closedir(sub);
          }
          closedir(dir);
        }
        tempInitialized = true;
      }

      int maxMilli = 0;
      char buf[16];

      for (int i = 0; i < tempFdCount; ++i) {
        lseek(tempFds[i], 0, SEEK_SET);
        ssize_t n = read(tempFds[i], buf, sizeof(buf) - 1);
        if (n <= 0) continue;

        buf[n] = '\0';
        int v = 0;
        for (char* p = buf; *p >= '0'; ++p)
          v = v * 10 + (*p - '0');

        if (v > maxMilli)
          maxMilli = v;
      }

      return maxMilli * 0.001f;
    }

    // ================= UPDATE =================
    void update() override {
      float ram  = getRamUsage();
      float cpu  = getCpuUsage();
      float temp = getCpuTemp();

      // ---- RAM ----
      ramElement.contentLen = snprintf(
        ramElement.content, CONTENT_MAX_LEN,
        "%s %.1f%% ▏", ICON_RAM, ram
      );
      ramElement.foregroundColor = (ram > RAM_WARN) ? colorAlert : colorNormal;
      ramElement.dirtyContent = true;

      // ---- CPU ----
      cpuElement.contentLen = snprintf(
        cpuElement.content, CONTENT_MAX_LEN,
        "%s %.1f%% ", ICON_CPU, cpu
      );
      cpuElement.foregroundColor = (cpu > CPU_WARN) ? colorAlert : colorNormal;
      cpuElement.dirtyContent = true;

      // ---- TEMP ----
      tempElement.contentLen = snprintf(
        tempElement.content, CONTENT_MAX_LEN,
        " %s %.0f°C", ICON_TEMP, temp
      );
      tempElement.foregroundColor = (temp > TEMP_WARN) ? colorAlert : colorNormal;
      tempElement.dirtyContent = true;

      lastUpdate = time(nullptr);
    }
};

#endif // RESOURCES_H
