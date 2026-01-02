#ifndef RESOURCES_H
#define RESOURCES_H

#include <stdio.h>
#include <string.h>
#include <fstream>
#include "module.h"

class ResourcesModule : public Module {
private:
    double last_total = 0, last_idle = 0;

    // Definición de iconos en bytes hexadecimales (UTF-8)
    static constexpr const char* ICON_RAM  = "\uefc5";
    static constexpr const char* ICON_CPU  = "\uf4bc";
    static constexpr const char* ICON_TEMP = "\uf2c9";

public:
    ResourcesModule() : Module("resources") {
        setSecondsPerUpdate(2);
    }

    float get_ram_usage() {
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

    float get_cpu_usage() {
        std::ifstream file("/proc/stat");
        std::string cpu_label;
        double u, n, s, i, io, irq, sirq, steal;
        if (!(file >> cpu_label >> u >> n >> s >> i >> io >> irq >> sirq >> steal)) return 0.0f;

        double idle_now = i + io;
        double total_now = u + n + s + i + io + irq + sirq + steal;
        double diff_total = total_now - last_total;
        double diff_idle = idle_now - last_idle;

        last_total = total_now;
        last_idle = idle_now;

        if (diff_total <= 0) return 0.0f;
        return (float)((1.0 - (diff_idle / diff_total)) * 100.0);
    }

    void update() override {
        float ram = get_ram_usage();
        float cpu = get_cpu_usage();

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

        char temp_buffer[128];
        
        // Formato con iconos coloreados según uso (solo los iconos)
        snprintf(temp_buffer, sizeof(temp_buffer),
                 "%%{F%s}%s%%{F-}%%{F%s} %.1f%% ▏ %%{F%s}%s%%{F-}%%{F%s} %.1f%% %%{F%s}%s%%{F-}%%{F%s} %.0f°C",
                 (ram > 80.0f) ? COLOR_RED : Module::COLOR_FG, ICON_RAM, Module::COLOR_FG, (double)ram,
                 (cpu > 80.0f) ? COLOR_RED : Module::COLOR_FG, ICON_CPU, Module::COLOR_FG, (double)cpu,
                 (temp > 70.0f) ? COLOR_RED : Module::COLOR_FG, ICON_TEMP, Module::COLOR_FG, (double)temp);

        buffer = temp_buffer;
    }

    void event(const char* eventValue) override {}
};

#endif
