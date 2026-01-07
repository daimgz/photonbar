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

BatteryModule() : Module("battery", false, 5) {
        // Configurar elemento del icono
        iconElement.moduleName = name;
        elements.push_back(&iconElement);

        // Configurar elemento del texto
        textElement.moduleName = name;
        elements.push_back(&textElement);
    }

    void update() override {
        update_battery_info();
    }

private:
    BarElement iconElement;
    BarElement textElement;
    long energy_now = 0, energy_full = 0, power_now = 0;
    char status[32] = "Unknown";
    float percentage = 0.0;

    void update_battery_info() {
        // Resetear valores
        energy_now = energy_full = power_now = 0;
        strcpy(status, "Unknown");
        percentage = 0.0;

        // Leer información del sistema
        FILE *f_now = fopen("/sys/class/power_supply/BAT0/energy_now", "r");
        if (!f_now) f_now = fopen("/sys/class/power_supply/BAT0/charge_now", "r");

        FILE *f_full = fopen("/sys/class/power_supply/BAT0/energy_full", "r");
        if (!f_full) f_full = fopen("/sys/class/power_supply/BAT0/charge_full", "r");

        FILE *f_pow = fopen("/sys/class/power_supply/BAT0/power_now", "r");
        if (!f_pow) f_pow = fopen("/sys/class/power_supply/BAT0/current_now", "r");

        FILE *f_stat = fopen("/sys/class/power_supply/BAT0/status", "r");

        // Obtener valores
        if (f_now && f_full) {
            fscanf(f_now, "%ld", &energy_now);
            fscanf(f_full, "%ld", &energy_full);
            if (energy_full > 0) percentage = ((float)energy_now / (float)energy_full) * 100.0;
            fclose(f_now); fclose(f_full);
        }
        if (f_pow) { fscanf(f_pow, "%ld", &power_now); fclose(f_pow); }
        if (f_stat) { fscanf(f_stat, "%s", status); fclose(f_stat); }

        // Obtener icono apropiado
        const char* icon = get_battery_icon();

        // Actualizar contenido de los elementos
        if (power_now > 0 && (strcmp(status, "Discharging") == 0 || strcmp(status, "Charging") == 0)) {
            // Mostrar tiempo restante o de carga
            float time_float;
            if (strcmp(status, "Discharging") == 0)
                time_float = (float)energy_now / power_now;
            else
                time_float = (float)(energy_full - energy_now) / power_now;

            int hours = (int)time_float;
            int minutes = (int)((time_float - hours) * 60);

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
        update_colors();

        lastUpdate = time(nullptr);
    }

    const char* get_battery_icon() {
        if (strcmp(status, "Charging") == 0) {
            if (percentage >= 95) return ICON_BATTERY_CHARGING_100;
            else if (percentage >= 90) return ICON_BATTERY_CHARGING_90;
            else if (percentage >= 80) return ICON_BATTERY_CHARGING_80;
            else if (percentage >= 70) return ICON_BATTERY_CHARGING_70;
            else if (percentage >= 60) return ICON_BATTERY_CHARGING_60;
            else if (percentage >= 50) return ICON_BATTERY_CHARGING_50;
            else if (percentage >= 40) return ICON_BATTERY_CHARGING_40;
            else if (percentage >= 30) return ICON_BATTERY_CHARGING_30;
            else if (percentage >= 20) return ICON_BATTERY_CHARGING_20;
            else if (percentage >= 10) return ICON_BATTERY_CHARGING_10;
            else return ICON_BATTERY_CHARGING_0;
        } else {
            if (percentage >= 95) return ICON_BATTERY_100;
            else if (percentage >= 90) return ICON_BATTERY_90;
            else if (percentage >= 80) return ICON_BATTERY_80;
            else if (percentage >= 70) return ICON_BATTERY_70;
            else if (percentage >= 60) return ICON_BATTERY_60;
            else if (percentage >= 50) return ICON_BATTERY_50;
            else if (percentage >= 40) return ICON_BATTERY_40;
            else if (percentage >= 30) return ICON_BATTERY_30;
            else if (percentage >= 20) return ICON_BATTERY_20;
            else if (percentage >= 10) return ICON_BATTERY_10;
            else return ICON_BATTERY_0;
        }
    }

void update_colors() {
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
