#ifndef BARELEMENT_H
#define BARELEMENT_H

#include "color.h"
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <cstring>

// Constantes para alineación (compatibles con bar.h)
#define ALIGN_L 0
#define ALIGN_C 1
#define ALIGN_R 2

enum class EventType {
    CLICK_LEFT = 1,
    CLICK_MIDDLE = 2,
    CLICK_RIGHT = 3,
    SCROLL_UP = 4,
    SCROLL_DOWN = 5
};

// Estructura optimizada para RAM/CPU - 5 punteros directos (40 bytes x64)
struct EventHandlers {
    std::function<void()> onClick;
    std::function<void()> onMiddleClick;
    std::function<void()> onRightClick;
    std::function<void()> onScrollUp;
    std::function<void()> onScrollDown;

    // Constructor para inicialización eficiente
    EventHandlers() : onClick(nullptr), onMiddleClick(nullptr),
                     onRightClick(nullptr), onScrollUp(nullptr), onScrollDown(nullptr) {}
};

struct BarElement {
    // --- Datos de texto (owned) ---
    char* content;
    uint32_t contentLen;

    // --- Datos de color ---
    Color foregroundColor;
    Color backgroundColor;
    Color underlineColor;

    // --- Manejo de eventos múltiples ---
    EventHandlers events;

    // --- Datos de formato ---
    int alignment;           // ALIGN_L/C/R
    int fontIndex;          // -1 = automático
    int offsetPixels;
    int screenTarget;       // +/-/f/l/número

    // --- Datos de posición (calculados) ---
    int beginX;
    int endX;

    // --- Estados y atributos ---
    bool underline;
    bool overline;
    bool reverseColors;     // para comando %R
    bool isActive;          // para áreas clickeables abiertas/cerradas

    // --- Métodos eficientes para manejo de eventos ---
    inline void setEvent(EventType type, std::function<void()> handler) {
        switch(type) {
            case EventType::CLICK_LEFT:   events.onClick = handler; break;
            case EventType::CLICK_MIDDLE: events.onMiddleClick = handler; break;
            case EventType::CLICK_RIGHT:  events.onRightClick = handler; break;
            case EventType::SCROLL_UP:    events.onScrollUp = handler; break;
            case EventType::SCROLL_DOWN:  events.onScrollDown = handler; break;
        }
    }

    // Constructor por defecto con valores inicializados
    BarElement() : content(nullptr), contentLen(0),
                   alignment(ALIGN_L), fontIndex(-1), offsetPixels(0), screenTarget(0),
                   beginX(0), endX(0), underline(false), overline(false),
                   reverseColors(false), isActive(false) {}

    // Constructor eficiente con texto
    BarElement(const char* text) : contentLen(strlen(text)),
                   alignment(ALIGN_L), fontIndex(-1), offsetPixels(0), screenTarget(0),
                   beginX(0), endX(0), underline(false), overline(false),
                   reverseColors(false), isActive(false) {
        content = (char*)malloc(contentLen + 1);
        if (content) {
            memcpy(content, text, contentLen);
            content[contentLen] = '\0';
        }
    }

    // Destructor automático para gestión de memoria
    ~BarElement() {
        if (content) {
            free(content);
            content = nullptr;
        }
    }

    // Prevenir copias inesperadas (RAII)
    BarElement(const BarElement&) = delete;
    BarElement& operator=(const BarElement&) = delete;

    // Mover semantics para eficiencia
    BarElement(BarElement&& other) noexcept : content(other.content), contentLen(other.contentLen),
                                            alignment(other.alignment), fontIndex(other.fontIndex),
                                            offsetPixels(other.offsetPixels), screenTarget(other.screenTarget),
                                            beginX(other.beginX), endX(other.endX),
                                            underline(other.underline), overline(other.overline),
                                            reverseColors(other.reverseColors), isActive(other.isActive),
                                            events(std::move(other.events)) {
        other.content = nullptr;
        other.contentLen = 0;
    }

    BarElement& operator=(BarElement&& other) noexcept {
        if (this != &other) {
            if (content) free(content);

            content = other.content;
            contentLen = other.contentLen;
            alignment = other.alignment;
            fontIndex = other.fontIndex;
            offsetPixels = other.offsetPixels;
            screenTarget = other.screenTarget;
            beginX = other.beginX;
            endX = other.endX;
            underline = other.underline;
            overline = other.overline;
            reverseColors = other.reverseColors;
            isActive = other.isActive;
            events = std::move(other.events);

            other.content = nullptr;
            other.contentLen = 0;
        }
        return *this;
    }
};

#endif // BARELEMENT_H
