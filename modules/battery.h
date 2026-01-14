#ifndef BATTERY_H
#define BATTERY_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sstream>
#include <iomanip>
#include "module.h"
#include "../helper.h"

class BatteryModule : public Module {
public:
BatteryModule() : Module("battery", false, 5) {
        // Configurar elemento del icono
        iconElement.moduleName = name;
        elements.push_back(&iconElement);

        // Configurar elemento del texto
        textElement.moduleName = name;
        elements.push_back(&textElement);
    }

    void update() override {
        updateBatteryInfo();
    }

private:
    BarElement iconElement;
    BarElement textElement;
    long energyNow = 0, energyFull = 0, powerNow = 0;
    char status[32] = "Unknown";
    float percentage = 0.0;

    void updateBatteryInfo() {
        // Resetear valores
        energyNow = energyFull = powerNow = 0;
        strcpy(status, "Unknown");
        percentage = 0.0;

        // Leer información del sistema
        FILE *f_now = fopen("/sys/class/power_supply/BAT0/energyNow", "r");
        if (!f_now) f_now = fopen("/sys/class/power_supply/BAT0/charge_now", "r");

        FILE *f_full = fopen("/sys/class/power_supply/BAT0/energyFull", "r");
        if (!f_full) f_full = fopen("/sys/class/power_supply/BAT0/charge_full", "r");

        FILE *f_pow = fopen("/sys/class/power_supply/BAT0/powerNow", "r");
        if (!f_pow) f_pow = fopen("/sys/class/power_supply/BAT0/current_now", "r");

        FILE *f_stat = fopen("/sys/class/power_supply/BAT0/status", "r");

        // Obtener valores
        if (f_now && f_full) {
            fscanf(f_now, "%ld", &energyNow);
            fscanf(f_full, "%ld", &energyFull);
            if (energyFull > 0) percentage = ((float)energyNow / (float)energyFull) * 100.0;
            fclose(f_now); fclose(f_full);
        }
        if (f_pow) { fscanf(f_pow, "%ld", &powerNow); fclose(f_pow); }
        if (f_stat) { fscanf(f_stat, "%s", status); fclose(f_stat); }

        // Obtener icono apropiado
        const char* icon = Helper::getBatteryIcon(
          percentage,
          strcmp(status, "Charging") == 0
        );

        // Actualizar contenido de los elementos
        if (powerNow > 0 && (strcmp(status, "Discharging") == 0 || strcmp(status, "Charging") == 0)) {
            // Mostrar tiempo restante o de carga
            float timeFloat;
            if (strcmp(status, "Discharging") == 0)
                timeFloat = (float)energyNow / powerNow;
            else
                timeFloat = (float)(energyFull - energyNow) / powerNow;

            int hours = (int)timeFloat;
            int minutes = (int)((timeFloat - hours) * 60);

            // Icono
            iconElement.contentLen = snprintf(
                iconElement.content,
                CONTENT_MAX_LEN,
                "%s ",
                icon
            );
            iconElement.content[iconElement.contentLen] = '\0';
            iconElement.dirtyContent = true;

            // Texto
            textElement.contentLen = snprintf(
                textElement.content,
                CONTENT_MAX_LEN,
                "%.1f%% %02d:%02d",
                percentage,
                hours,
                minutes
            );
            textElement.content[textElement.contentLen] = '\0';
            textElement.dirtyContent = true;
        } else {
            // Solo mostrar porcentaje
            // Icono
            iconElement.contentLen = snprintf(
                iconElement.content,
                CONTENT_MAX_LEN,
                "%s",
                icon
            );
            iconElement.content[iconElement.contentLen] = '\0';
            iconElement.dirtyContent = true;

            // Texto
            textElement.contentLen = snprintf(
                textElement.content,
                CONTENT_MAX_LEN,
                "%.1f%%",
                percentage
            );
            textElement.content[textElement.contentLen] = '\0';
            textElement.dirtyContent = true;
        }

        // Actualizar color según estado
        updateColors();

        lastUpdate = time(nullptr);
    }

void updateColors() {
        // Color del texto: siempre morado (para ambos elementos)
        textElement.foregroundColor = Color::parse_color("#E0AAFF", NULL, Color(224, 170, 255, 255)); // Morado

        // Color del icono: según estado de carga
        if (strcmp(status, "Charging") == 0) {
            int r, g, b;

            if (percentage >= 90) {
                // Verde puro al 90% o más
                r = 0; g = 255; b = 0;
            } else if (percentage >= 20) {
                // Naranja entre 20% y 90%
                r = 255; g = 165; b = 0;
            } else {
                // Rojo si está bajo 20% mientras carga
                r = 255; g = 0; b = 0;
            }

            std::stringstream ss;
            ss << "#" << std::hex << std::setw(2) << std::setfill('0') << r
               << std::setw(2) << std::setfill('0') << g
               << std::setw(2) << std::setfill('0') << b;

            // Aplicar color SOLO al icono de carga
            iconElement.foregroundColor = Color::parse_color(ss.str().c_str(), NULL, Color(r, g, b, 255));
        } else {
            // Si no está cargando, icono en morado normal
            iconElement.foregroundColor = Color::parse_color("#E0AAFF", NULL, Color(224, 170, 255, 255)); // Morado
        }

        // Texto en rojo si batería baja y no está cargando
        if (percentage < 20 && strcmp(status, "Charging") != 0) {
            textElement.foregroundColor = Color::parse_color("#FF0000", NULL, Color(255, 0, 0, 255)); // Rojo
        }
    }
};

#endif // BATTERY_H
