#ifndef TIMER_H
#define TIMER_H

#include <chrono>
#include <stdio.h>
#include <string.h>
#include "module.h"
#include "../color.h"

class TimerModule : public Module {
private:
    // 5 elementos principales
    BarElement baseElement;
    BarElement playIconElement;
    BarElement hourElement;
    BarElement minuteElement;
    BarElement secondElement;

    // Gestión de elementos
    std::vector<BarElement*> timeElements;
    std::vector<BarElement*> allElements;

    // Estado del timer (igual que el sistema viejo)
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point pause_time;
    bool is_running = false;
    bool is_paused = false;
    bool show_details = false;
    long long accumulated_pause_time = 0;
    long long target_duration_ms = 0;

    static constexpr const char* ICON_PLAY = " \uf04b";
    static constexpr const char* ICON_PAUSE = " \uf04c";
    static constexpr const char* ICON_LOGO = u8"\U000f06ad";

    // Ya no necesitamos esta función - el timer debe seguir corriendo visualmente

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

    void adjustTime(long long ms) {
        if (is_running) return;

        target_duration_ms += ms;
        if (target_duration_ms < 0) target_duration_ms = 0;

        update();
        if (renderFunction) renderFunction();
    }

    void playPause() {
        if (!is_running) {
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

        update();
        if (renderFunction) renderFunction();
    }

    void reset() {
        // Solo bloquear reset si está corriendo y el tiempo NO se ha agotado
        if (is_running && !is_paused && get_remaining_ms() > 0) return;
        
        accumulated_pause_time = 0;
        target_duration_ms = 0;
        
        // Cambiar estados al final
        is_running = false;
        is_paused = false;

        update();
        if (renderFunction) renderFunction();
    }

    void toggleDetails() {
        show_details = !show_details;

        update();
        if (renderFunction) renderFunction();
    }

    void setupAllElements() {
        // Configurar moduleName para todos los elementos
        baseElement.moduleName = name;
        playIconElement.moduleName = name;
        hourElement.moduleName = name;
        minuteElement.moduleName = name;
        secondElement.moduleName = name;
        
        // Almacenar referencias
        allElements = {&baseElement, &playIconElement, &hourElement, 
                       &minuteElement, &secondElement};
        timeElements = {&hourElement, &minuteElement, &secondElement};
        
        // Agregar TODOS los elementos al vector (nunca se modifica)
        elements.push_back(&baseElement);
        elements.push_back(&playIconElement);
        elements.push_back(&hourElement);
        elements.push_back(&minuteElement);
        elements.push_back(&secondElement);
    }

    void configureAllEvents() {
        // Configurar clicks en TODOS los elementos
        for (auto* elem : allElements) {
            elem->setEvent(BarElement::CLICK_LEFT, [this]() { playPause(); });
            elem->setEvent(BarElement::CLICK_MIDDLE, [this]() { reset(); });
            elem->setEvent(BarElement::CLICK_RIGHT, [this]() { toggleDetails(); });
        }

        // Scroll SOLO en elementos de tiempo
        hourElement.setEvent(BarElement::SCROLL_UP, [this]() { adjustTime(3600000); });
        hourElement.setEvent(BarElement::SCROLL_DOWN, [this]() { adjustTime(-3600000); });

        minuteElement.setEvent(BarElement::SCROLL_UP, [this]() { adjustTime(60000); });
        minuteElement.setEvent(BarElement::SCROLL_DOWN, [this]() { adjustTime(-60000); });

        secondElement.setEvent(BarElement::SCROLL_UP, [this]() { adjustTime(1000); });
        secondElement.setEvent(BarElement::SCROLL_DOWN, [this]() { adjustTime(-1000); });
    }

    // Ya no necesitamos esta función - la visibilidad se maneja con contentLen = 0

    void applyColors() {
        Color defaultColor = Color::parse_color("#E0AAFF", NULL, Color(224, 170, 255, 255));
        Color redColor = Color::parse_color("#FF0000", NULL, Color(255, 0, 0, 255));
        Color orangeColor = Color::parse_color("#FFA500", NULL, Color(255, 165, 0, 255));
        Color greenColor = Color::parse_color("#90EE90", NULL, Color(144, 238, 144, 255));

        // Aplicar colores a todos los elementos según estado
        for (auto* elem : allElements) {
            if (!is_running) {
                elem->foregroundColor = defaultColor;
            } else if (is_paused) {
                elem->foregroundColor = orangeColor;
            } else if (get_remaining_ms() == 0) {
                elem->foregroundColor = redColor;
            } else {
                elem->foregroundColor = greenColor;
            }
        }
    }

public:
    TimerModule() : Module("timer", false, 1) {
        setupAllElements();
        configureAllEvents();
    }

    ~TimerModule() {
        // Los elementos son estáticos, no necesitan limpieza manual
        // Solo limpiamos las referencias si fuera necesario
        timeElements.clear();
        allElements.clear();
    }

    void update() override {
        applyColors();

        long long remaining = get_remaining_ms();
        int hours = remaining / 3600000;
        int minutes = (remaining % 3600000) / 60000;
        int seconds = (remaining % 60000) / 1000;

        // Logo (siempre visible)
        baseElement.contentLen = snprintf(
            baseElement.content,
            CONTENT_MAX_LEN,
            "%s",
            ICON_LOGO
        );
        
        // En modo oculto, el logo es estático
        baseElement.dirtyContent = show_details;

        // Icono play/pause (solo visible cuando show_details = true)
        if (show_details) {
            if (!is_running) {
                // Cuando no está corriendo pero se muestra, mostrar play
                playIconElement.contentLen = snprintf(
                    playIconElement.content,
                    CONTENT_MAX_LEN,
                    "%s ",
                    ICON_PLAY
                );
            } else {
                // Cuando está corriendo, mostrar estado actual
                const char* icon = is_paused ? ICON_PLAY : ICON_PAUSE;
                playIconElement.contentLen = snprintf(
                    playIconElement.content,
                    CONTENT_MAX_LEN,
                    "%s ",
                    icon
                );
            }
            playIconElement.dirtyContent = true;
        } else {
            // Ocultar playIcon cuando está en modo oculto
            playIconElement.contentLen = snprintf(playIconElement.content, CONTENT_MAX_LEN, "");
            playIconElement.dirtyContent = false;  // No necesita actualización en modo oculto
        }

        // Elementos de tiempo
        if (show_details) {
            hourElement.contentLen = snprintf(
                hourElement.content,
                CONTENT_MAX_LEN,
                "-%02d:",
                hours
            );
            hourElement.dirtyContent = true;

            minuteElement.contentLen = snprintf(
                minuteElement.content,
                CONTENT_MAX_LEN,
                "%02d:",
                minutes
            );
            minuteElement.dirtyContent = true;

            secondElement.contentLen = snprintf(
                secondElement.content,
                CONTENT_MAX_LEN,
                "%02d",
                seconds
            );
            secondElement.dirtyContent = true;
        } else {
            // Ocultar elementos de tiempo cuando está en modo oculto
            hourElement.contentLen = snprintf(hourElement.content, CONTENT_MAX_LEN, "");
            hourElement.dirtyContent = false;  // No necesita actualización en modo oculto
            
            minuteElement.contentLen = snprintf(minuteElement.content, CONTENT_MAX_LEN, "");
            minuteElement.dirtyContent = false;  // No necesita actualización en modo oculto
            
            secondElement.contentLen = snprintf(secondElement.content, CONTENT_MAX_LEN, "");
            secondElement.dirtyContent = false;  // No necesita actualización en modo oculto
        }

        // Actualizar timestamp - CRÍTICO
        lastUpdate = time(nullptr);
    }
};

#endif
