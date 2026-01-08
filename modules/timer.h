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
    std::chrono::steady_clock::time_point startTime;
    std::chrono::steady_clock::time_point pauseTime;
    bool isRunning = false;
    bool isPaused = false;
    bool showDetails = false;
    long long accumulatedPauseTime = 0;
    long long targetDurationMs = 0;

    static constexpr const char* ICON_PLAY = " \uf04b";
    static constexpr const char* ICON_PAUSE = " \uf04c";
    static constexpr const char* ICON_LOGO = u8"\U000f06ad";

    // Ya no necesitamos esta función - el timer debe seguir corriendo visualmente

    long long getRemainingMs() {
        if (!isRunning) return targetDurationMs;

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime);

        long long actualElapsed;
        if (isPaused) {
            auto pause_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(pauseTime - startTime);
            actualElapsed = pause_elapsed.count() - accumulatedPauseTime;
        } else {
            actualElapsed = elapsed.count() - accumulatedPauseTime;
        }

        long long remaining = targetDurationMs - actualElapsed;
        return remaining > 0 ? remaining : 0;
    }

    void adjustTime(long long ms) {
        if (isRunning) return;

        targetDurationMs += ms;
        if (targetDurationMs < 0) targetDurationMs = 0;

        update();
        if (renderFunction) renderFunction();
    }

    void playPause() {
        if (!isRunning) {
            if (targetDurationMs > 0) {
                startTime = std::chrono::steady_clock::now();
                isRunning = true;
                isPaused = false;
                accumulatedPauseTime = 0;
            }
        } else if (isPaused) {
            auto now = std::chrono::steady_clock::now();
            accumulatedPauseTime += std::chrono::duration_cast<std::chrono::milliseconds>(now - pauseTime).count();
            isPaused = false;
        } else {
            pauseTime = std::chrono::steady_clock::now();
            isPaused = true;
        }

        update();
        if (renderFunction) renderFunction();
    }

    void reset() {
        // Solo bloquear reset si está corriendo y el tiempo NO se ha agotado
        if (isRunning && !isPaused && getRemainingMs() > 0) return;
        
        accumulatedPauseTime = 0;
        targetDurationMs = 0;
        
        // Cambiar estados al final
        isRunning = false;
        isPaused = false;

        update();
        if (renderFunction) renderFunction();
    }

    void toggleDetails() {
        showDetails = !showDetails;

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
            if (!isRunning) {
                elem->foregroundColor = defaultColor;
            } else if (isPaused) {
                elem->foregroundColor = orangeColor;
            } else if (getRemainingMs() == 0) {
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

        long long remaining = getRemainingMs();
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
        
        // El logo siempre necesita actualizarse por cambios de color
        baseElement.dirtyContent = true;

        // Icono play/pause (solo visible cuando showDetails = true)
        if (showDetails) {
            if (!isRunning) {
                // Cuando no está corriendo pero se muestra, mostrar play
                playIconElement.contentLen = snprintf(
                    playIconElement.content,
                    CONTENT_MAX_LEN,
                    "%s ",
                    ICON_PLAY
                );
            } else {
                // Cuando está corriendo, mostrar estado actual
                const char* icon = isPaused ? ICON_PLAY : ICON_PAUSE;
                playIconElement.contentLen = snprintf(
                    playIconElement.content,
                    CONTENT_MAX_LEN,
                    "%s ",
                    icon
                );
            }
            playIconElement.dirtyContent = true;
        } else {
            // Ocultar playIcon cuando está en modo oculto (forzar actualización)
            playIconElement.contentLen = snprintf(playIconElement.content, CONTENT_MAX_LEN, "");
            playIconElement.dirtyContent = true;  // Forzar actualización para ocultar
        }

        // Elementos de tiempo
        if (showDetails) {
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
            hourElement.dirtyContent = true;  // Forzar actualización para ocultar
            
            minuteElement.contentLen = snprintf(minuteElement.content, CONTENT_MAX_LEN, "");
            minuteElement.dirtyContent = true;  // Forzar actualización para ocultar
            
            secondElement.contentLen = snprintf(secondElement.content, CONTENT_MAX_LEN, "");
            secondElement.dirtyContent = true;  // Forzar actualización para ocultar
        }

        // Actualizar timestamp - CRÍTICO
        lastUpdate = time(nullptr);
    }
};

#endif
