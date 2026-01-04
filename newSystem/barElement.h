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

#define CONTENT_MAX_LEN 255

typedef std::function<void()> EventFunction;

struct BarElement {
    // --- Datos de texto (owned) ---
  //private:
    char content[CONTENT_MAX_LEN];
    uint32_t ucsContent[CONTENT_MAX_LEN];
    uint8_t  ucsContentCharWidths[CONTENT_MAX_LEN];
    bool dirtyContent;
    uint8_t contentLen;
    uint8_t ucsContentLen;

    // --- Datos de posición (calculados) ---
    uint16_t beginX;
    uint16_t widthX;

  //public:
    //const char *getContent() const { return content; }
    //uint8_t getContentLen() const { return contentLen; }
    //uint16_t getBeginX() const { return beginX; }
    //uint16_t getWidthX() const { return widthX; }

    //void setContent(const char *str, const uint8_t len) {
        //this->contentLen = len;
        //strcpy(content, str);

        //std::memset(ucsContent, 0, sizeof(ucsContent));
        //dirtyContent = true;
    //}

    enum EventType {
        CLICK_LEFT = 1,
        CLICK_MIDDLE = 2,
        CLICK_RIGHT = 3,
        SCROLL_UP = 4,
        SCROLL_DOWN = 5
    };

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


    // --- Estados y atributos ---
    bool underline;
    bool overline;
    bool reverseColors;     // para comando %R
    bool isActive;          // para áreas clickeables abiertas/cerradas

    // --- Métodos eficientes para manejo de eventos ---
    inline void setEvent(EventType type, std::function<void()> handler) {
        events[type] = handler;
    }
    bool eventCharged;

    // Constructor por defecto con valores inicializados
    BarElement() : content(""), dirtyContent(false), contentLen(0),
                   beginX(0), widthX(0), alignment(ALIGN_L), fontIndex(-1),
                   offsetPixels(0), screenTarget(0), underline(false), overline(false),
                   reverseColors(false), isActive(false), eventCharged(false) {}

};

#endif // BARELEMENT_H
