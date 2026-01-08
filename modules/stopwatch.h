#ifndef STOPWATCH_H
#define STOPWATCH_H

#include <chrono>
#include <stdio.h>
#include <string.h>
#include "module.h"
#include "../color.h"

class StopwatchModule : public Module {
private:
    // 2 elementos principales: logo y tiempo
    BarElement baseElement;
    BarElement timeElement;
    
    // Gestión de elementos
    std::vector<BarElement*> allElements;
    
    // Estado del stopwatch
    std::chrono::steady_clock::time_point startTime;
    std::chrono::steady_clock::time_point pauseTime;
    bool isRunning = false;
    bool isPaused = false;
    bool showDetails = false;
    long long accumulatedPauseTime = 0;

    static constexpr const char* ICON_PLAY = " \uf04b";
    static constexpr const char* ICON_PAUSE = " \uf04c";
    static constexpr const char* ICON_LOGO = "\uf2f2"; // Icono de reloj más común

    long long get_elapsed_ms() {
        if (!isRunning) return 0;

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime);

        if (isPaused) {
            // Si está pausado, el tiempo transcurrido es hasta la pausa
            auto pause_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(pauseTime - startTime);
            return pause_elapsed.count() - accumulatedPauseTime;
        } else {
            return elapsed.count() - accumulatedPauseTime;
        }
    }

    void formatTime(char* buffer, size_t bufferSize, long long elapsedMs) {
        int hours = elapsedMs / 3600000;
        int minutes = (elapsedMs % 3600000) / 60000;
        int seconds = (elapsedMs % 60000) / 1000;

        snprintf(buffer, bufferSize, "%02d:%02d:%02d", hours, minutes, seconds);
    }

    void playPause() {
        if (!isRunning) {
            // Iniciar
            startTime = std::chrono::steady_clock::now();
            isRunning = true;
            isPaused = false;
            accumulatedPauseTime = 0;
        } else if (isPaused) {
            // Reanudar
            auto now = std::chrono::steady_clock::now();
            accumulatedPauseTime += std::chrono::duration_cast<std::chrono::milliseconds>(now - pauseTime).count();
            isPaused = false;
        } else {
            // Pausar
            pauseTime = std::chrono::steady_clock::now();
            isPaused = true;
        }

        update();
        if (renderFunction) renderFunction();
    }

    void reset() {
        // Resetear stopwatch (siempre permitido)
        isRunning = false;
        isPaused = false;
        accumulatedPauseTime = 0;

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
        timeElement.moduleName = name;
        
        // Almacenar referencias
        allElements = {&baseElement, &timeElement};
        
        // Agregar TODOS los elementos al vector (nunca se modifica)
        elements.push_back(&baseElement);
        elements.push_back(&timeElement);
    }

    void configureAllEvents() {
        // Configurar clicks en TODOS los elementos
        for (auto* elem : allElements) {
            elem->setEvent(BarElement::CLICK_LEFT, [this]() { playPause(); });
            elem->setEvent(BarElement::CLICK_MIDDLE, [this]() { reset(); });
            elem->setEvent(BarElement::CLICK_RIGHT, [this]() { toggleDetails(); });
        }
        
        // Stopwatch no tiene eventos de scroll (no ajusta tiempo)
    }

    void applyColors() {
        Color defaultColor = Color::parse_color("#E0AAFF", NULL, Color(224, 170, 255, 255));
        Color orangeColor = Color::parse_color("#FFA500", NULL, Color(255, 165, 0, 255));
        Color greenColor = Color::parse_color("#90EE90", NULL, Color(144, 238, 144, 255));
        
        // Aplicar colores a todos los elementos según estado
        for (auto* elem : allElements) {
            if (!isRunning) {
                elem->foregroundColor = defaultColor;
            } else if (isPaused) {
                elem->foregroundColor = orangeColor;
            } else {
                elem->foregroundColor = greenColor;
            }
        }
    }

public:
    StopwatchModule() : Module("stopwatch", false, 1) {
        setupAllElements();
        configureAllEvents();
    }

    ~StopwatchModule() {
        // Los elementos son estáticos, no necesitan limpieza manual
        allElements.clear();
    }

    void update() override {
        applyColors();

        long long elapsed = get_elapsed_ms();
        char time_buffer[64];
        formatTime(time_buffer, sizeof(time_buffer), elapsed);

        // Logo (siempre visible)
        baseElement.contentLen = snprintf(
            baseElement.content,
            CONTENT_MAX_LEN,
            "%s",
            ICON_LOGO
        );
        
        // El logo siempre necesita actualizarse por cambios de color
        baseElement.dirtyContent = true;

        // Elemento de tiempo (solo visible en modo detallado)
        if (showDetails) {
            const char* action_icon = "";
            if (isRunning) {
                action_icon = isPaused ? ICON_PLAY : ICON_PAUSE;
            }
            
            timeElement.contentLen = snprintf(
                timeElement.content,
                CONTENT_MAX_LEN,
                "%s %s",
                action_icon,
                time_buffer
            );
            timeElement.dirtyContent = true;
        } else {
            // Ocultar elemento de tiempo en modo compacto
            timeElement.contentLen = snprintf(timeElement.content, CONTENT_MAX_LEN, "");
            timeElement.dirtyContent = true;  // Forzar actualización para ocultar
        }

        // Actualizar timestamp - CRÍTICO
        lastUpdate = time(nullptr);
    }
};

#endif // STOPWATCH_H