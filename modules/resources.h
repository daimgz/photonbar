#ifndef RESOURCES_H
#define RESOURCES_H

#include <stdio.h>
#include <string.h>
#include <fstream>
#include "module.h"
#include "../barElement.h"
#include "../color.h"

class ResourcesModule : public Module {
  private:
    double lastTotal = 0, lastIdle = 0;

    // Elementos de la UI
    BarElement ramElement;
    BarElement cpuElement;
    BarElement tempElement;

    // Definición de iconos en bytes hexadecimales (UTF-8)
    static constexpr const char* ICON_RAM  = "\uefc5";
    static constexpr const char* ICON_CPU  = "\uf4bc";
    static constexpr const char* ICON_TEMP = "\uf2c9";

  public:
    ResourcesModule() : Module("resources", false, 2) {  // No auto-update, cada 2 segundos
      // Configurar elemento RAM
      ramElement.moduleName = name;
      ramElement.foregroundColor = Color::parse_color("#E0AAFF", NULL, Color(224, 170, 255, 255));
      elements.push_back(&ramElement);

      // Configurar elemento CPU
      cpuElement.moduleName = name;
      cpuElement.foregroundColor = Color::parse_color("#E0AAFF", NULL, Color(224, 170, 255, 255));
      elements.push_back(&cpuElement);

      // Configurar elemento Temperatura
      tempElement.moduleName = name;
      tempElement.foregroundColor = Color::parse_color("#E0AAFF", NULL, Color(224, 170, 255, 255));
      elements.push_back(&tempElement);
    }

    float getRamUsage() {
      std::ifstream file("/proc/meminfo");
      std::string line;
      double total = 0, avail = 0;
      while (std::getline(file, line)) {
        if (line.find("MemTotal:") == 0) sscanf(line.c_str(), "MemTotal: %lf", &total);
        if (line.find("MemAvailable:") == 0) sscanf(line.c_str(), "MemAvailable: %lf", &avail);
      }
      if (total <= 0) return 0.0f;
      return (float)((1.0 - (avail / total)) * 100.0);
    }

    float getCpuUsage() {
      std::ifstream file("/proc/stat");
      std::string cpu_label;
      double u, n, s, i, io, irq, sirq, steal;
      if (!(file >> cpu_label >> u >> n >> s >> i >> io >> irq >> sirq >> steal)) return 0.0f;

      double idle_now = i + io;
      double total_now = u + n + s + i + io + irq + sirq + steal;
      double diff_total = total_now - lastTotal;
      double diff_idle = idle_now - lastIdle;

      lastTotal = total_now;
      lastIdle = idle_now;

      if (diff_total <= 0) return 0.0f;
      return (float)((1.0 - (diff_idle / diff_total)) * 100.0);
    }

    void update() override {
      float ram = getRamUsage();
      float cpu = getCpuUsage();

      float temp = 0.0f;

      // Intentar obtener temperatura usando el comando 'sensors' como en tu script Python
      FILE* pipe = popen("sensors 2>/dev/null", "r");
      if (pipe) {
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
          // Buscar líneas con "Core" como en tu script
          if (strstr(buffer, "Core") != NULL && strchr(buffer, ':') != NULL) {
            char* temp_str = strchr(buffer, ':') + 1;
            // Extraer temperatura (ej: "55.0°C")
            float temp_val = 0.0f;
            if (sscanf(temp_str, "%f°C", &temp_val) == 1) {
              temp = temp_val;
              break; // Usar primera temperatura encontrada
            }
          }
        }
        pclose(pipe);
      }

      // Actualizar elemento RAM
      ramElement.contentLen = snprintf(
        ramElement.content,
        CONTENT_MAX_LEN,
        "%s %.1f%% ▏",
        ICON_RAM, ram
      );
      ramElement.dirtyContent = true;
      // Color según uso de RAM
      ramElement.foregroundColor = (ram > 80.0f) ?
        Color::parse_color("#FF6B6B", NULL, Color(255, 107, 107, 255)) :  // Rojo
        Color::parse_color("#E0AAFF", NULL, Color(224, 170, 255, 255));  // Normal

      // Actualizar elemento CPU
      cpuElement.contentLen = snprintf(
        cpuElement.content,
        CONTENT_MAX_LEN,
        "%s %.1f%% ",
        ICON_CPU, cpu
      );
      cpuElement.dirtyContent = true;
      // Color según uso de CPU
      cpuElement.foregroundColor = (cpu > 80.0f) ?
        Color::parse_color("#FF6B6B", NULL, Color(255, 107, 107, 255)) :  // Rojo
        Color::parse_color("#E0AAFF", NULL, Color(224, 170, 255, 255));  // Normal

      // Actualizar elemento Temperatura
      tempElement.contentLen = snprintf(
        tempElement.content,
        CONTENT_MAX_LEN,
        " %s %.0f°C",
        ICON_TEMP, temp
      );
      tempElement.dirtyContent = true;
      // Color según temperatura
      tempElement.foregroundColor = (temp > 70.0f) ?
        Color::parse_color("#FF6B6B", NULL, Color(255, 107, 107, 255)) :  // Rojo
        Color::parse_color("#E0AAFF", NULL, Color(224, 170, 255, 255));  // Normal

      // 🔥 CRÍTICO: Actualizar timestamp
      lastUpdate = time(nullptr);
    }
};

#endif // RESOURCES_H
