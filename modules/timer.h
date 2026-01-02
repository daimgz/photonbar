#ifndef TIMER_H
#define TIMER_H

#include <chrono>
#include <stdio.h>
#include <string.h>
#include "module.h"

class TimerModule : public Module {
private:
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point pause_time;
    bool is_running = false;
    bool is_paused = false;
    bool show_details = false;
    long long accumulated_pause_time = 0;
    long long target_duration_ms = 0; // Inicializado en 0 por default

    static constexpr const char* ICON_PLAY = " \uf04b";
    static constexpr const char* ICON_PAUSE = " \uf04c";
    static constexpr const char* ICON_LOGO = "\ue384";

    long long get_remaining_ms() {
        if (!is_running) return target_duration_ms;

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time);

        long long actual_elapsed;
        if (is_paused) {
            auto pause_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(pause_time - start_time);
            actual_elapsed = pause_elapsed.count() - accumulated_pause_time;
        } else {
            actual_elapsed = elapsed.count() - accumulated_pause_time;
        }

        long long remaining = target_duration_ms - actual_elapsed;
        return remaining > 0 ? remaining : 0;
    }

    void adjust_time(long long ms) {
        // Solo permitir modificar si el timer no está en ejecución
        if (is_running) return;

        target_duration_ms += ms;
        if (target_duration_ms < 0) target_duration_ms = 0;
        markForUpdate();
    }

public:
    // Constructor por defecto inicializa todo en 0
    TimerModule() : Module("timer") {
        target_duration_ms = 0;
        setSecondsPerUpdate(1);
        setUpdatePerIteration(true);
    }

    void update() override {
        const char* action_icon = "";
        static char color_tag[32];

        if (!is_running) {
            sprintf(color_tag, "%%{F%s}", Module::COLOR_FG);
        } else if (is_paused) {
            sprintf(color_tag, "%%{F#FFA500}");
            action_icon = ICON_PLAY;
        } else {
            sprintf(color_tag, "%%{F#90EE90}");
            action_icon = ICON_PAUSE;
        }

        // Color rojo si el tiempo se agota
        if (is_running && !is_paused && get_remaining_ms() == 0) {
            sprintf(color_tag, "%%{F#FF0000}");
        }

        // --- MODO COMPACTO ---
        if (!show_details) {
            std::string base = "%{A2:timer_reset:}%{A3:timer_toggle:}";
            if (!is_running) {
                base += "%{A4:timer_min_up:}%{A5:timer_min_down:}";
            }
            base += color_tag + std::string(ICON_LOGO) + "%{F-}%{A}%{A}";
            if (!is_running) base += "%{A}%{A}";
            buffer = base;
            return;
        }

        // --- MODO DETALLADO ---
        long long remaining = get_remaining_ms();
        int hours = remaining / 3600000;
        int minutes = (remaining % 3600000) / 60000;
        int seconds = (remaining % 60000) / 1000;

        char temp_buffer[1024];
        char h_str[128], m_str[128], s_str[128];

        if (!is_running) {
            // Generar áreas de scroll solo si no está corriendo
            snprintf(h_str, sizeof(h_str), "%%{A4:timer_hr_up:}%%{A5:timer_hr_down:}-%02d%%{A}%%{A}", hours);
            snprintf(m_str, sizeof(m_str), "%%{A4:timer_min_up:}%%{A5:timer_min_down:}%02d%%{A}%%{A}", minutes);
            snprintf(s_str, sizeof(s_str), "%%{A4:timer_sec_up:}%%{A5:timer_sec_down:}%02d%%{A}%%{A}", seconds);
        } else {
            // Texto estático mientras corre
            snprintf(h_str, sizeof(h_str), "-%02d", hours);
            snprintf(m_str, sizeof(m_str), "%02d", minutes);
            snprintf(s_str, sizeof(s_str), "%02d", seconds);
        }

        snprintf(temp_buffer, sizeof(temp_buffer),
                "%%{A1:timer_play_pause:}%%{A2:timer_reset:}%%{A3:timer_toggle:}%s%s%s %s:%s:%s%%{F-}%%{A}%%{A}%%{A}",
                color_tag, ICON_LOGO, action_icon, h_str, m_str, s_str);

        buffer = temp_buffer;
    }

    void event(const char* eventValue) override {
        if (strstr(eventValue, "timer_reset")) {
            // Volver todo a cero y detener
            is_running = false;
            is_paused = false;
            accumulated_pause_time = 0;
            target_duration_ms = 0;
        }
        else if (strstr(eventValue, "timer_play_pause")) {
            if (!is_running) {
                // Solo inicia si el usuario configuró algún tiempo > 0
                if (target_duration_ms > 0) {
                    start_time = std::chrono::steady_clock::now();
                    is_running = true;
                    is_paused = false;
                    accumulated_pause_time = 0;
                }
            } else if (is_paused) {
                auto now = std::chrono::steady_clock::now();
                accumulated_pause_time += std::chrono::duration_cast<std::chrono::milliseconds>(now - pause_time).count();
                is_paused = false;
            } else {
                pause_time = std::chrono::steady_clock::now();
                is_paused = true;
            }
        }
        else if (strstr(eventValue, "timer_toggle")) {
            show_details = !show_details;
        }
        // Eventos de scroll
        else if (strstr(eventValue, "timer_hr_up"))   adjust_time(3600000);
        else if (strstr(eventValue, "timer_hr_down")) adjust_time(-3600000);
        else if (strstr(eventValue, "timer_min_up"))  adjust_time(60000);
        else if (strstr(eventValue, "timer_min_down")) adjust_time(-60000);
        else if (strstr(eventValue, "timer_sec_up"))  adjust_time(1000);
        else if (strstr(eventValue, "timer_sec_down")) adjust_time(-1000);

        markForUpdate();
    }
};

#endif
