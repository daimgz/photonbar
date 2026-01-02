#ifndef STOPWATCH_H
#define STOPWATCH_H

#include <chrono>
#include <stdio.h>
#include <string.h>
#include "module.h"

class StopwatchModule : public Module {
private:
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point pause_time;
    bool is_running = false;
    bool is_paused = false;
    bool show_details = false;
    long long accumulated_pause_time = 0;

    // Iconos de Nerd Fonts para play/pausa
    static constexpr const char* ICON_PLAY = " \uf04b";
    static constexpr const char* ICON_PAUSE = " \uf04c";
    static constexpr const char* ICON_LOGO = "\uf2f2"; // Icono de reloj más común

    long long get_elapsed_ms() {
        if (!is_running) return 0;

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time);

        if (is_paused) {
            // Si está pausado, el tiempo transcurrido es hasta la pausa
            auto pause_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(pause_time - start_time);
            return pause_elapsed.count() - accumulated_pause_time;
        } else {
            return elapsed.count() - accumulated_pause_time;
        }
    }

    void format_time(char* buffer, size_t buffer_size, long long elapsed_ms) {
        int hours = elapsed_ms / 3600000;
        int minutes = (elapsed_ms % 3600000) / 60000;
        int seconds = (elapsed_ms % 60000) / 1000;

        snprintf(buffer, buffer_size, "%02d:%02d:%02d", hours, minutes, seconds);
    }

public:
    StopwatchModule() : Module("stopwatch") {
        setSecondsPerUpdate(1);
        setUpdatePerIteration(true);
    }

    void update() override {
        const char* action_icon = "";
        static char color_tag[32];

        if (!is_running) {
            // Reset estado: solo logo, sin color especial
            sprintf(color_tag, "%%{F%s}", Module::COLOR_FG);
        } else if (is_paused) {
            // Pausado: logo + play con color naranja
            sprintf(color_tag, "%%{F#FFA500}");
            action_icon = ICON_PLAY;
        } else {
            // Reproduciendo: logo + pause con color verde
            sprintf(color_tag, "%%{F#90EE90}");
            action_icon = ICON_PAUSE;
        }

        if (!show_details) {
            // Modo compacto: siempre muestra logo (con color de estado)
            buffer = std::string("%{A2:stopwatch_reset:}%{A3:stopwatch_toggle:}") +
                    color_tag + ICON_LOGO + "%{F-}%{A}%{A}";
            return;
        }

        long long elapsed = get_elapsed_ms();
        char time_buffer[64];
        format_time(time_buffer, sizeof(time_buffer), elapsed);

        char temp_buffer[256];
        snprintf(temp_buffer, sizeof(temp_buffer),
                "%%{A1:stopwatch_play_pause:}%%{A2:stopwatch_reset:}%%{A3:stopwatch_toggle:}%s%s%s %s%%{F-}%%{A}%%{A}%%{A}",
                color_tag, ICON_LOGO, action_icon, time_buffer);

        buffer = temp_buffer;
    }

    void event(const char* eventValue) override {
        if (strstr(eventValue, "stopwatch_reset")) {
            // Resetear stopwatch (click central)
            is_running = false;
            is_paused = false;
            accumulated_pause_time = 0;

        } else if (strstr(eventValue, "stopwatch_play_pause")) {
            // Pausar/Reanudar (click izquierdo)
            if (!is_running) {
                // Iniciar
                start_time = std::chrono::steady_clock::now();
                is_running = true;
                is_paused = false;
                accumulated_pause_time = 0;
            } else if (is_paused) {
                // Reanudar
                auto now = std::chrono::steady_clock::now();
                accumulated_pause_time += std::chrono::duration_cast<std::chrono::milliseconds>(now - pause_time).count();
                is_paused = false;
            } else {
                // Pausar
                pause_time = std::chrono::steady_clock::now();
                is_paused = true;
            }

        } else if (strstr(eventValue, "stopwatch_toggle")) {
            // Mostrar/ocultar detalles (click derecho)
            show_details = !show_details;
        }

        markForUpdate();
    }
};

#endif // STOPWATCH_H
