#ifndef BARELEMENT_H
#define BARELEMENT_H

#include "color.h"
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <cstring>
#include <map>
#include <string>
#include <xcb/xproto.h>

// Constantes para alineación (compatibles con bar.h)
#define ALIGN_L 0
#define ALIGN_C 1
#define ALIGN_R 2


typedef std::function<void()> EventFunction;

struct BarElement {
    // --- Datos de texto (owned) ---
enum EventType {
    CLICK_LEFT = 1,
    CLICK_MIDDLE = 2,
    CLICK_RIGHT = 3,
    SCROLL_UP = 4,
    SCROLL_DOWN = 5
};
    char content[100];
    uint32_t contentLen;

    // --- Datos de color ---
    Color foregroundColor;
    Color backgroundColor;
    Color underlineColor;

    // --- Manejo de eventos múltiples ---
    std::map<EventType, EventFunction> events;
    std::string moduleName;

    //TODO: esto debe pasar a ser por modulo que por elemento
    xcb_window_t window;

    // --- Datos de formato ---
    int alignment;           // ALIGN_L/C/R
    int fontIndex;          // -1 = automático
    int offsetPixels;
    int screenTarget;       // +/-/f/l/número

    // --- Datos de posición (calculados) ---
    unsigned int beginX: 16;
    unsigned int endX: 16;

    // --- Estados y atributos ---
    bool underline;
    bool overline;
    bool reverseColors;     // para comando %R
    bool isActive;          // para áreas clickeables abiertas/cerradas

    // --- Métodos eficientes para manejo de eventos ---
    inline void setEvent(EventType type, std::function<void()> handler) {
        events[type] = handler;
    }

    // Constructor por defecto con valores inicializados
    BarElement() : content(""), contentLen(0),
                   alignment(ALIGN_L), fontIndex(-1), offsetPixels(0), screenTarget(0),
                   beginX(0), endX(0), underline(false), overline(false),
                   reverseColors(false), isActive(false) {}

    // Constructor eficiente con texto
    //BarElement(const char* text) : contentLen(strlen(text)),
                   //alignment(ALIGN_L), fontIndex(-1), offsetPixels(0), screenTarget(0),
                   //beginX(0), endX(0), underline(false), overline(false),
                   //reverseColors(false), isActive(false) {
        //content = (char*)malloc(contentLen + 1);
        //if (content) {
            //memcpy(content, text, contentLen);
            //content[contentLen] = '\0';
        //}
    //}

    // Destructor automático para gestión de memoria
    ~BarElement() {
        //if (content) {
            //free(content);
            //content = nullptr;
        //}
    }

    // Prevenir copias inesperadas (RAII)
    BarElement(const BarElement&) = delete;
    BarElement& operator=(const BarElement&) = delete;

    // Mover semantics para eficiencia
    BarElement(BarElement&& other) noexcept : contentLen(other.contentLen),
                                            events(std::move(other.events)), alignment(other.alignment),
                                            fontIndex(other.fontIndex), offsetPixels(other.offsetPixels),
                                            screenTarget(other.screenTarget), beginX(other.beginX),
                                            endX(other.endX), underline(other.underline),
                                            overline(other.overline), reverseColors(other.reverseColors),
                                            isActive(other.isActive) {
        strcpy(content, other.content);
        //other.content = nullptr;
        //other.contentLen = 0;
    }

    BarElement& operator=(BarElement&& other) noexcept {
        if (this != &other) {
            //if (content) free(content);

            strcpy(content, other.content);
            //content = other.content;
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

            //other.content = nullptr;
            //other.contentLen = 0;
        }
        return *this;
    }
};

#endif // BARELEMENT_H
