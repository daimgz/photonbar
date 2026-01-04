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

class BarElement {
    // --- Datos de texto (owned) ---
  private:
    char content[CONTENT_MAX_LEN];
    uint32_t ucsContent[CONTENT_MAX_LEN];
    uint8_t contentLen;

    // --- Datos de posición (calculados) ---
    uint16_t beginX;
    uint16_t widthX;


  public:
    const char *getContent() const { return content; }
    uint8_t getContentLen() const { return contentLen; }
    uint16_t getBeginX() const { return beginX; }
    uint16_t getWidthX() const { return widthX; }

    void setContent(const char *str, const uint8_t len) {
        this->contentLen = len;
        strcpy(content, str);

        std::memset(ucsContent, 0, sizeof(ucsContent));

        int ucsCount = 0;
        for (;;) {
            if (*str == '\0' || *str == '\n') {
                ucsContent[ucsCount] = '\0';
                break;
            }

            uint8_t *utf = (uint8_t *)str;
            uint32_t ucs; // Código Unicode de 32 bits (soporta emoji/iconos)

            // === DECODIFICACIÓN UTF-8 ===
            // ASCII (1 byte): 0xxxxxxx
            if (utf[0] < 0x80) {
                ucs = utf[0];
                str  += 1;
            }
            // UTF-8 de 2 bytes: 110xxxxx 10xxxxxx
            else if ((utf[0] & 0xe0) == 0xc0) {
                ucs = (utf[0] & 0x1f) << 6 | (utf[1] & 0x3f);
                str += 2;
            }
            // UTF-8 de 3 bytes: 1110xxxx 10xxxxxx 10xxxxxx
            else if ((utf[0] & 0xf0) == 0xe0) {
                ucs = (utf[0] & 0xf) << 12 | (utf[1] & 0x3f) << 6 | (utf[2] & 0x3f);
                str += 3;
            }
            // UTF-8 de 4 bytes: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
            // AQUÍ ES DONDE ESTÁN LOS ICONOS NUEVOS (emoji, símbolos Unicode extendidos)
            else if ((utf[0] & 0xf8) == 0xf0) {
                ucs = (utf[0] & 0x07) << 18 | (utf[1] & 0x3f) << 12 | (utf[2] & 0x3f) << 6 | (utf[3] & 0x3f);
                str += 4;
            }
            // Secuencias de 5 y 6 bytes son obsoletas en el estándar Unicode actual
            else {
                ucs = utf[0];
                str += 1;
            }

            ucsContent[ucsCount] = ucs;
            ucsCount++;


            // === SELECCIÓN DE FUENTE APROPIADA ===
            cur_font = select_drawable_font(ucs);
            if (!cur_font)
                continue;  // Si ninguna fuente tiene el glifo, omitir carácter

            // === CONFIGURACIÓN DE FUENTE EN CONTEXTO GRÁFICO ===
            if(cur_font->ptr)  // Solo para fuentes XCB (no Xft)
                xcb_change_gc(c, gc[GC_DRAW] , XCB_GC_FONT, (const uint32_t []) {
                cur_font->ptr
            });

            // === DIBUJAR CARÁCTER ===
            //TODO: obtener el width de algu manera
            //int w = draw_char(cur_mon, cur_font, pos_x, align, ucs);

            // === ACTUALIZAR POSICIÓN Y ÁREAS ===
            pos_x += w;  // Avanza posición X según ancho del carácter
            area_shift(cur_mon->window, align, w);  // Ajusta áreas clickeables
                //std::cout << "ucs: " << ucs << ", w: " << w << std::endl;
        }

    }

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

    // Constructor por defecto con valores inicializados
    BarElement() : content(""), contentLen(0),
                   alignment(ALIGN_L), fontIndex(-1), offsetPixels(0), screenTarget(0),
                   beginX(0), endX(0), underline(false), overline(false),
                   reverseColors(false), isActive(false) {}

};

#endif // BARELEMENT_H
