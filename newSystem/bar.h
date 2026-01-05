// vim:sw=4:ts=4:et:
#include "barElement.h"
#ifndef LEMONBAR_LIB
#include <vector>
#define _POSIX_C_SOURCE 200809L

// For C++ compilation with X11 headers
//#ifdef __cplusplus
//extern "C" {
//#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <poll.h>
#include <getopt.h>
#include <unistd.h>
#include <errno.h>
#include <xcb/xcb.h>
#include <xcb/xcbext.h>
#if WITH_XINERAMA
#include <xcb/xinerama.h>
#endif
#include <xcb/randr.h>

#include <X11/Xft/Xft.h>
#include <X11/Xlib-xcb.h>

#include <string>
#include <vector>
#include <functional>

#include <iostream>
#include <string>
//#ifdef __cplusplus
//}
//#endif

// Here be dragons

#ifndef VERSION
#define VERSION "dev"
#endif

/* Forward declarations for functions used before their definitions */
//void xconn(void);
//void init(char *wm_name, char *wm_instance);

#define max(a,b) ((a) > (b) ? (a) : (b))
#define min(a,b) ((a) < (b) ? (a) : (b))
#define indexof(c,s) (strchr((s),(c))-(s))

typedef struct font_t {
    xcb_font_t ptr;
    xcb_charinfo_t *width_lut;

    XftFont *xft_ft;

    int ascent;

    int descent, height, width;
    uint32_t char_max;
    uint32_t char_min;
} font_t;

typedef struct monitor_t {
    int x, y, width;
    xcb_window_t window;
    xcb_pixmap_t pixmap;
    struct monitor_t *prev, *next;
} monitor_t;

//typedef struct area_t {
    //unsigned int begin:16;
    //unsigned int end:16;
    //bool active:1;
    //int align:3;
    //unsigned int button:3;
    //xcb_window_t window;
    //char *cmd;
//} area_t;


//typedef struct area_stack_t {
    //int at, max;
    //area_t *area;
//} area_stack_t;

enum {
    ATTR_OVERL = (1<<0),
    ATTR_UNDERL = (1<<1),
};

//enum { ALIGN_L = 0,
       //ALIGN_C,
       //ALIGN_R
     //};

enum {
    GC_DRAW = 0,
    GC_CLEAR,
    GC_ATTR,
    GC_MAX
};

#define MAX_FONT_COUNT 5
//char width lookuptable
#define MAX_WIDTHS (1 << 16)

class Bar{


private:
wchar_t xft_char[MAX_WIDTHS];
char    xft_width[MAX_WIDTHS];
XftColor sel_fg;
XftDraw *xft_draw;
    Display *dpy;
xcb_connection_t *c;

xcb_screen_t *scr;
int scr_nbr = 0;

xcb_gcontext_t gc[GC_MAX];
xcb_visualid_t visual;
Visual *visual_ptr;
xcb_colormap_t colormap;
int visual_depth = 0; // depth of chosen visual (bits)


monitor_t *monhead, *montail;
font_t *font_list[MAX_FONT_COUNT];
int font_count = 0;
int font_index = -1;
int offsets_y[MAX_FONT_COUNT];
int offset_y_count = 0;
int offset_y_index = 0;

uint32_t attrs = 0;
bool dock = false; // hace que la barra siempre sea visible en la pantalla
bool topbar = true;
int bw = -1, bh = -1, bx = 0, by = 0;
int bu = 1; // Underline height
Color
    foregroundColor,
    backgroundColor,
    underlineColor;

Color
    defaultForegroundColor,
    defaultBackgroundColor,
    defaultUnderlineColor;

//area_stack_t area_stack;

std::function<void(const char *cmd)> click_cb;

/* Flag to prevent infinite EXPOSE loop */
bool processing_expose = false;

const std::vector<BarElement*> elements;

public:

    Bar(
        const char *name,
        const char *_backgroundColor,
        const char *_foregroundColor,
        const bool topBar,
        const std::vector<std::string> &fonts,
        const std::vector<BarElement*> &barElements
    ) :
        topbar(topBar),
        elements(barElements)
    {
        //connect();
        for (const auto& font : fonts) {
            font_load(font.c_str());
        }
        //setClickHandler(cb);
//Color::parse_color(hex, NULL, (Color)0x00000000U)
        // Ensure sensible default colors for in-process use (opaque)
        defaultBackgroundColor = backgroundColor = Color::parse_color(_backgroundColor, NULL, (Color)0x00000000U);
        defaultForegroundColor = foregroundColor = Color::parse_color(_foregroundColor, NULL, (Color)0x11111111U);
        defaultUnderlineColor = underlineColor = foregroundColor;

        // Connect to X and initialize
        xconn();
        fprintf(stderr, "[lemonbar] lemonbar_init_lib: xconn complete\n");

        // init() expects fonts to be loaded already; caller should call font_load()
        init((char *)name, (char *)name);
        fprintf(stderr, "[lemonbar] lemonbar_init_lib: init complete\n");

        //setBackground(_backgroundColor);
    }

void setClickHandler(std::function<void(const char *cmd)> cb) {
    click_cb = cb;
}

void
update_gc (void)
{
    xcb_change_gc(c, gc[GC_DRAW], XCB_GC_FOREGROUND, (const uint32_t []){ foregroundColor.v });
    xcb_change_gc(c, gc[GC_CLEAR], XCB_GC_FOREGROUND, (const uint32_t []){ backgroundColor.v });
    xcb_change_gc(c, gc[GC_ATTR], XCB_GC_FOREGROUND, (const uint32_t []){ underlineColor.v });
    XftColorFree(dpy, visual_ptr, colormap , &sel_fg);
    char color[] = "#ffffff";
    uint32_t nfgc = foregroundColor.v & 0x00ffffff;
    snprintf(color, sizeof(color), "#%06X", nfgc);
    if (!XftColorAllocName (dpy, visual_ptr, colormap, color, &sel_fg)) {
        fprintf(stderr, "Couldn't allocate xft font color '%s'\n", color);
    }
}

void
fill_gradient (xcb_drawable_t d, int x, int y, int width, int height, Color start, Color stop)
{
    float i;
    const int K = 25; // The number of steps

    for (i = 0.; i < 1.; i += (1. / K)) {
        // Perform the linear interpolation magic
        unsigned int rr = i * stop.r + (1. - i) * start.r;
        unsigned int gg = i * stop.g + (1. - i) * start.g;
        unsigned int bb = i * stop.b + (1. - i) * start.b;

        // The alpha is ignored here
        Color step = {
            static_cast<uint8_t>(bb),
            static_cast<uint8_t>(gg),
            static_cast<uint8_t>(rr),
            255,
        };

        xcb_change_gc(c, gc[GC_DRAW], XCB_GC_FOREGROUND, (const uint32_t []){ step.v });
        xcb_poly_fill_rectangle(
            c,
            d,
            gc[GC_DRAW],
            1,
            (const xcb_rectangle_t []){
                {
                    static_cast<int16_t>(x),
                    static_cast<int16_t>(i * bh),
                    static_cast<uint16_t>(width),
                    static_cast<uint16_t>(bh / K + 1)
                }
            }
        );
    }

    xcb_change_gc(c, gc[GC_DRAW], XCB_GC_FOREGROUND, (const uint32_t []){ foregroundColor.v });
}

void
fill_rect (xcb_drawable_t d, xcb_gcontext_t _gc, int x, int y, int width, int height)
{
    xcb_poly_fill_rectangle(c, d, _gc, 1, (const xcb_rectangle_t []){ { x, y, width, height } });
}

// Apparently xcb cannot seem to compose the right request for this call, hence we have to do it by
// ourselves.
// The funcion is taken from 'wmdia' (http://wmdia.sourceforge.net/)
xcb_void_cookie_t xcb_poly_text_16_simple(xcb_connection_t * c,
    xcb_drawable_t drawable, xcb_gcontext_t gc, int16_t x, int16_t y,
    uint32_t len, const uint16_t *str)
{
    static const xcb_protocol_request_t xcb_req = {
        5,                // count
        0,                // ext
        XCB_POLY_TEXT_16, // opcode
        1                 // isvoid
    };
    struct iovec xcb_parts[7];
    uint8_t xcb_lendelta[2];
    xcb_void_cookie_t xcb_ret;
    xcb_poly_text_8_request_t xcb_out;

    xcb_out.pad0 = 0;
    xcb_out.drawable = drawable;
    xcb_out.gc = gc;
    xcb_out.x = x;
    xcb_out.y = y;

    xcb_lendelta[0] = len;
    xcb_lendelta[1] = 0;

    xcb_parts[2].iov_base = (char *)&xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_parts[4].iov_base = xcb_lendelta;
    xcb_parts[4].iov_len = sizeof(xcb_lendelta);
    xcb_parts[5].iov_base = (char *)str;
    xcb_parts[5].iov_len = len * sizeof(int16_t);

    xcb_parts[6].iov_base = 0;
    xcb_parts[6].iov_len = -(xcb_parts[4].iov_len + xcb_parts[5].iov_len) & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);

    return xcb_ret;
}


int
xft_char_width_slot (uint32_t ch)
{
    int slot = ch % MAX_WIDTHS;
    while (xft_char[slot] != 0 && xft_char[slot] != ch)
    {
        slot = (slot + 1) % MAX_WIDTHS;
    }
    return slot;
}

int xft_char_width (uint32_t ch, font_t *cur_font)
{
    int slot = xft_char_width_slot(ch);
    if (!xft_char[slot]) {
        XGlyphInfo gi;
        FT_UInt glyph = XftCharIndex (dpy, cur_font->xft_ft, (FcChar32) ch);
        XftFontLoadGlyphs (dpy, cur_font->xft_ft, FcFalse, &glyph, 1);
        XftGlyphExtents (dpy, cur_font->xft_ft, &glyph, 1, &gi);
        XftFontUnloadGlyphs (dpy, cur_font->xft_ft, &glyph, 1);
        xft_char[slot] = ch;
        if (gi.xOff >= gi.width) {
            xft_width[slot] = gi.xOff;
        } else {
            xft_width[slot] = gi.width;
        }
        return xft_width[slot];
    } else if (xft_char[slot] == ch)
        return xft_width[slot];
    else
        return 0;
}

int
shift (monitor_t *mon, int x, int align, int ch_width)
{
    switch (align) {
        case ALIGN_C:
            xcb_copy_area(c, mon->pixmap, mon->pixmap, gc[GC_DRAW],
                    mon->width / 2 - x / 2, 0,
                    mon->width / 2 - (x + ch_width) / 2, 0,
                    x, bh);
            x = mon->width / 2 - (x + ch_width) / 2 + x;
            break;
        case ALIGN_R:
            xcb_copy_area(c, mon->pixmap, mon->pixmap, gc[GC_DRAW],
                    mon->width - x, 0,
                    mon->width - x - ch_width, 0,
                    x, bh);
            x = mon->width - ch_width;
            break;
    }

        /* Draw the background first */
    fill_rect(mon->pixmap, gc[GC_CLEAR], x, 0, ch_width, bh);
    return x;
}

void
draw_lines (monitor_t *mon, int x, int w)
{
    /* We can render both at the same time */
    if (attrs & ATTR_OVERL)
        fill_rect(mon->pixmap, gc[GC_ATTR], x, 0, w, bu);
    if (attrs & ATTR_UNDERL)
        fill_rect(mon->pixmap, gc[GC_ATTR], x, bh - bu, w, bu);
}

void
draw_shift (monitor_t *mon, int x, int align, int w)
{
    x = shift(mon, x, align, w);
    draw_lines(mon, x, w);
}

int
draw_char (monitor_t *mon, font_t *cur_font, int x, int align, uint32_t ch)
{
    int ch_width;

    if (cur_font->xft_ft) {
        ch_width = xft_char_width(ch, cur_font);
    } else {
        ch_width = (cur_font->width_lut) ?
            cur_font->width_lut[ch - cur_font->char_min].character_width:
            cur_font->width;
    }

    x = shift(mon, x, align, ch_width);

    int y = bh / 2 + cur_font->height / 2- cur_font->descent + offsets_y[offset_y_index];
    if (cur_font->xft_ft) {
        // Para Xft usa la versión de 32 bits que soporta tus iconos grandes
        XftDrawString32(xft_draw, &sel_fg, cur_font->xft_ft, x, y, (const FcChar32 *)&ch, 1);
    } else {
        // Para XCB, solo si el icono cabe en 16 bits
        if (ch <= 0xFFFF) {
            uint16_t ch16 = (uint16_t)ch;
            // XCB requiere Big Endian para texto de 16 bits, hay que swappear
            ch16 = (ch16 >> 8) | (ch16 << 8);
            xcb_poly_text_16_simple(c, mon->pixmap, gc[GC_DRAW], x, y, 1, &ch16);
        }
    }

    draw_lines(mon, x, ch_width);

    return ch_width;
}


void
set_attribute (const char modifier, const char attribute)
{
    int pos = indexof(attribute, "ou");

    if (pos < 0) {
        fprintf(stderr, "Invalid attribute \"%c\" found\n", attribute);
        return;
    }

    switch (modifier) {
    case '+':
        attrs |= (1u<<pos);
        break;
    case '-':
        attrs &=~(1u<<pos);
        break;
    case '!':
        attrs ^= (1u<<pos);
        break;
    }
}


//area_t *
//area_get (xcb_window_t win, const int btn, const int x)
//{
    //// Looping backwards ensures that we get the innermost area first
    //for (int i = area_stack.at - 1; i >= 0; i--) {
        //area_t *a = &area_stack.area[i];
        //if (a->window == win && a->button == btn && x >= a->begin && x < a->end)
            //return a;
    //}
    //return NULL;
//}

void area_shift (xcb_window_t win, const int align, int delta)
{
    if (align == ALIGN_L)
        return;
    if (align == ALIGN_C)
        delta /= 2;

    for (BarElement* e : elements) {
        if (e->window == win && e->alignment == align && !e->isActive) {
            e->beginX -= delta;   // ✓ Correcto
            // e->widthX -= delta;  // ❌ Error: modifica ancho, no coordenada final
            // En su lugar, widthX debe permanecer igual (es el ancho del elemento)
        }
    }
}

bool
font_has_glyph (font_t *font, const uint32_t c)
{
    if (font->xft_ft) {
        if (XftCharExists(dpy, font->xft_ft, (FcChar32) c)) {
            return true;
        } else {
            return false;
        }

    }

    if (c < font->char_min || c > font->char_max)
        return false;

    if (font->width_lut && font->width_lut[c - font->char_min].character_width == 0)
        return false;

    return true;
}

font_t *
select_drawable_font (const uint32_t c)
{
    // If the user has specified a font to use, try that first.
    if (font_index != -1 && font_has_glyph(font_list[font_index - 1], c)) {
        offset_y_index = font_index - 1;
        return font_list[font_index - 1];
    }

    // If the end is reached without finding an appropriate font, return NULL.
    // If the font can draw the character, return it.
    for (int i = 0; i < font_count; i++) {
        if (font_has_glyph(font_list[i], c)) {
            offset_y_index = i;
            return font_list[i];
        }
    }
    return NULL;
}



void parseBarElement (std::vector<BarElement*> *elements)
{
    std::cout << std::endl << std::endl << "[BarManager] parseBarElement: received '" << elements << "'" << std::endl << std::endl <<std::endl;
    // === VARIABLES LOCALES DE ESTADO ===
    font_t *cur_font;        // Fuente actual seleccionada para dibujar
    monitor_t *cur_mon;      // Monitor actual donde se está dibujando
    int pos_x, align; // Posición X actual, alineación, botón del mouse
    //char *block_end, *ep; // Punteros: actual al texto, fin del bloque, fin de parámetro
    Color tmp;               // Variable temporal para intercambiar colores

    // === INICIALIZACIÓN DE ESTADO DE DIBUJO ===
    pos_x = 0;              // Posición X inicial (comienza desde la izquierda)
    align = ALIGN_L;         // Alineación inicial: izquierda
    cur_mon = monhead;      // Comenzar con el primer monitor

    // === REINICIALIZACIÓN DEL STACK DE ÁREAS CLICKEABLES ===
    // Reinicia el contador de áreas para limpiar áreas anteriores
    //area_stack.at = 0;

    // === LIMPIEZA DE TODOS LOS MONITORES ===
    // Limpia el pixmap de cada monitor con el color de fondo
    for (monitor_t *m = monhead; m != NULL; m = m->next)
        fill_rect(m->pixmap, gc[GC_CLEAR], 0, 0, m->width, bh);

    // === CREACIÓN DEL DRAWABLE XFT ===memset(ptr, '\0', sizeof(data));
    // Xft drawable permite dibujar texto con fuentes TrueType/OpenType
    if (!(xft_draw = XftDrawCreate (dpy, cur_mon->pixmap, visual_ptr , colormap))) {
        fprintf(stderr, "Couldn't create xft drawable\n");
    }

    //area_t *a = nullptr;
        //std::cout << std::endl << std::endl << "estoy" << std::endl << std::endl << std::endl;
    // === BUCLE PRINCIPAL DE PARSEO ===
    // Procesa cada carácter o bloque de formato del texto
    for (BarElement* element : *elements) {
        //std::cout << std::endl << std::endl << "estoy" << element->content << " len " << element->contentLen << std::endl << std::endl << std::endl;
        element->beginX = pos_x;
        // === COMANDOS DE COLOR ===
        //case 'B': backgroundColor = Color::parse_color(p, &p, defaultBackgroundColor); update_gc(); break;
        if (element->backgroundColor != Color(0x00000000U))
            backgroundColor = element->backgroundColor;
        //case 'F': foregroundColor = Color::parse_color(p, &p, defaultForegroundColor); update_gc(); break;
        if (element->foregroundColor != Color(0x00000000U))
            foregroundColor = element->foregroundColor;
        //case 'U': underlineColor = Color::parse_color(p, &p, defaultUnderlineColor); update_gc(); break;
        if (element->underlineColor != Color(0x00000000U))
            underlineColor = element->underlineColor;

        element->window = cur_mon->window;
        update_gc();
        // === CONDICIÓN DE SALIDA ===
        // Termina si encuentra fin de string o salto de línea
        //if (*p == '\0' || *p == '\n')
			//break;

        // === DETECCIÓN DE BLOQUE DE FORMATO %{...} ===
        // Si encuentra '%' seguido de '{', es un comando de formato
        //if (p[0] == '%' && p[1] == '{' && (block_end = strchr(p++, '}'))) {
            //p++;  // Salta el '{'

            // === PROCESAMIENTO DE COMANDOS DENTRO DEL BLOQUE ===
            // Itera sobre cada comando dentro de %{...}
            //while (p < block_end) {
                //int w;  // Variable para anchos temporales
                //while (isspace(*p))
                    //p++;  // Omite espacios en blanco

                // === SWITCH DE COMANDOS DE FORMATO ===
                // Cada carácter dentro de %{...} es un comando diferente
                //switch (*p++) {
                    // === COMANDOS DE ATRIBUTOS DE TEXTO ===
                    //case '+': set_attribute('+', *p++); break;  // Activar atributo (subrayado/sobrelineado)
                    //case '-': set_attribute('-', *p++); break;  // Desactivar atributo
                    //case '!': set_attribute('!', *p++); break;  // Alternar atributo

                    // === COMANDO DE INVERSIÓN DE COLORES ===
                    // TODO:habilitar inversion de colores

                    //case 'R':
                              //tmp = foregroundColor;           // Intercambia colores
                              //foregroundColor = backgroundColor;
                              //backgroundColor = tmp;
                              //update_gc();                    // Actualiza contextos gráficos
                              //break;

                    // === COMANDOS DE ALINEACIÓN ===
                    //case 'l': pos_x = 0; align = ALIGN_L; break;  // Alinear izquierda
                    //case 'c': pos_x = 0; align = ALIGN_C; break;  // Alinear centro
                    //case 'r': pos_x = 0; align = ALIGN_R; break;  // Alinear derecha
                    //pos_x = 0;
                    //align = element->alignment;

                    // === COMANDO DE ÁREA CLICKEABLE ===
                    //case 'A':
                        //button = XCB_BUTTON_INDEX_1;  // Por defecto: botón izquierdo
                        //// Permite especificar botón 1-5 después de 'A'
                        //if (isdigit(*p) && (*p > '0' && *p < '6'))
                            //button = *p++ - '0';      // Convierte '1'-'5' a 1-5
                        //// Llama a area_add para crear área clicable
                        //if (!area_add(p, block_end, &p, cur_mon, pos_x, align, button))
                            //return;  // Error en area_add, terminar parseo

                        //int areaId = 0;
                        //if (element->eventCharged == false) {
                            //std::cout << "event charged" << std::endl << std::endl << std::endl;
                            //for (std::pair<BarElement::EventType, EventFunction> pair : element->events) {
                                ////areaId = area_stack.at++;
                                //a = &area_stack.area[areaId];  // Reserva espacio para nueva área
                                //std::string str =
                                    //std::string(element->moduleName) +
                                    //"-" +
                                    //std::to_string((uint)pair.first);

                                //a->begin = pos_x;

                                //a->active = true;
                                //a->align = align;
                                //a->button = (uint)pair.first;
                                //a->window = cur_mon->window;
                                ////a->cmd = str.c_str();
                                //a->cmd = (char*)malloc(str.size() + 1);
                                //strcpy(a->cmd, str.c_str());

                                //std::cout<< "area start: " << a->begin << std::endl;
                                //std::cout <<"area end: " << a->end << std::endl;
                                //std::cout<< "area cmd" << a->cmd << std::endl;
                                ////button = pair.first;
                            //}
                        //}
                        //break;


                    // === COMANDO DE SELECCIÓN DE MONITOR ===
                    //case 'S':
                              //// S+: siguiente monitor
                              //if (*p == '+' && cur_mon->next)
                              //{ cur_mon = cur_mon->next; }
                              //// S-: monitor anterior
                              //else if (*p == '-' && cur_mon->prev)
                              //{ cur_mon = cur_mon->prev; }
                              //// Sf: primer monitor (first)
                              //else if (*p == 'f')
                              //{ cur_mon = monhead; }
                              //// Sl: último monitor (last)
                              //else if (*p == 'l')
                              //{ cur_mon = montail ? montail : monhead; }
                              //// S0-S9: monitor por índice
                              //else if (isdigit(*p))
                              //{ cur_mon = monhead;
                                //for (int i = 0; i != *p-'0' && cur_mon->next; i++)
                                    //cur_mon = cur_mon->next;
                              //}
                              //else
                              //{ p++; continue; }  // Comando inválido, omitir
							  //XftDrawDestroy (xft_draw);  // Destruye drawable antiguo
							  //if (!(xft_draw = XftDrawCreate (dpy, cur_mon->pixmap, visual_ptr , colormap ))) {
								//fprintf(stderr, "Couldn't create xft drawable\n");
//}

                              //p++;  // Salta el caracter de selección
                              //pos_x = 0;  // Reinicia posición X para nuevo monitor
                              //break;

                    // === COMANDO DE DESPLAZAMIENTO/OFFSET ===
                    //case 'O':
                              //errno = 0;  // Limpia errno para detección de errores
                              //w = (int) strtoul(p, &p, 10);  // Parsea número de píxeles
                              //if (errno)
                                  //continue;  // Error en conversión, omitir

                              //// Dibuja rectángulo con background y desplaza contenido
                              //draw_shift(cur_mon, pos_x, align, w);

                              //pos_x += w;  // Actualiza posición X
                              //area_shift(cur_mon->window, align, w);  // Ajusta áreas clickeables
                              //break;

                    // === COMANDO DE SELECCIÓN DE FUENTE ===
                    //case 'T':
                              //if (*p == '-') { //T-: volver a selección automática
                                  //font_index = -1;
                                  //p++;
                                  //break;
                              //} else if (isdigit(*p)) {
                                  //font_index = (int)strtoul(p, &ep, 10);
                                  //// Valida que el índice esté en rango (1, font_count]
                                  //if (!font_index || font_index > font_count)
                                  //font_index = -1;  // Fuera de rango, usar automático
                                  //p = ep;  // Avanza puntero al final del número
                                  //break;
                              //} else {
                                  //fprintf(stderr, "Invalid font slot \"%c\"\n", *p++); //Swallow the token
                                  //break;
                              //}

                    // === MANEJO DE COMANDOS DESCONOCIDOS ===
                    // En caso de error, continúa después del '}'
                    //default:
                        //p = block_end;  // Salta al final del bloque
                //}
                //pos_x += element->contentLen;
            //}
            // === CONSUME EL '}' DE CIERRE ===
            //p++;  // Salta el carácter '}' que cierra el bloque de formato
        //} else {
            // === PROCESAMIENTO DE TEXTO NORMAL (UTF-8) ===
            // Convierte UTF-8 a UCS-4 para procesamiento interno
        //for (char p : element->content) {
        //std::cout << std::endl << std::endl << "antes del for" << std::endl << std::endl << std::endl;
        //char *
        char *p = element->content;

        if (element->dirtyContent) {
            uint8_t char_width = 0;
            for (uint8_t i = 0;;i++) {
                if (*p == '\0' || *p == '\n') {
                    element->ucsContent[i] = '\0';
                    element->ucsContentLen = i;
                    break;
                }
            //std::cout << std::endl << std::endl << "dentro del for" << std::endl << std::endl << std::endl;

            //std::cout << std::endl << std::endl << p << std::endl << std::endl << std::endl;

                uint8_t *utf = (uint8_t *)p;
                uint32_t ucs; // Código Unicode de 32 bits (soporta emoji/iconos)

                // === DECODIFICACIÓN UTF-8 ===
                // ASCII (1 byte): 0xxxxxxx
                if (utf[0] < 0x80) {
                    ucs = utf[0];
                    p  += 1;
                }
                // UTF-8 de 2 bytes: 110xxxxx 10xxxxxx
                else if ((utf[0] & 0xe0) == 0xc0) {
                    ucs = (utf[0] & 0x1f) << 6 | (utf[1] & 0x3f);
                    p += 2;
                }
                // UTF-8 de 3 bytes: 1110xxxx 10xxxxxx 10xxxxxx
                else if ((utf[0] & 0xf0) == 0xe0) {
                    ucs = (utf[0] & 0xf) << 12 | (utf[1] & 0x3f) << 6 | (utf[2] & 0x3f);
                    p += 3;
                }
                // UTF-8 de 4 bytes: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
                // AQUÍ ES DONDE ESTÁN LOS ICONOS NUEVOS (emoji, símbolos Unicode extendidos)
                else if ((utf[0] & 0xf8) == 0xf0) {
                    ucs = (utf[0] & 0x07) << 18 | (utf[1] & 0x3f) << 12 | (utf[2] & 0x3f) << 6 | (utf[3] & 0x3f);
                    p += 4;
                }
                // Secuencias de 5 y 6 bytes son obsoletas en el estándar Unicode actual
                else {
                    ucs = utf[0];
                    p += 1;
                }

                // === SELECCIÓN DE FUENTE APROPIADA ===
                cur_font = select_drawable_font(ucs);
                if (!cur_font)
                    ucs = '?';
                    //continue;

                // === CONFIGURACIÓN DE FUENTE EN CONTEXTO GRÁFICO ===
                //if(cur_font->ptr)  // Solo para fuentes XCB (no Xft)
                    //xcb_change_gc(c, gc[GC_DRAW] , XCB_GC_FONT, (const uint32_t []) {
                    //cur_font->ptr
                //});

                // === DIBUJAR CARÁCTER ===
                //TODO: ver hacer calculo de width sin dibujar
                    //
                //int w = draw_char(cur_mon, cur_font, pos_x, align, ucs);
                if (cur_font->xft_ft) {
                    char_width = xft_char_width(ucs, cur_font);
                } else {
                    char_width = (cur_font->width_lut) ?
                        cur_font->width_lut[ucs - cur_font->char_min].character_width:
                        cur_font->width;
                }

                element->ucsContent[i] = ucs;

                // === ACTUALIZAR POSICIÓN Y ÁREAS ===
                pos_x += char_width;  // Avanza posición X según ancho del carácter
                area_shift(cur_mon->window, align, char_width);  // Ajusta áreas clickeables
                element->ucsContentCharWidths[i] = char_width;
                std::cout << "ucs: " << ucs << ", w: " << char_width << std::endl;
            }
            element->width = pos_x - element->beginX;
            std::cout << "element->widthX: " << element->width << std::endl;
            std::cout << "pos_x: " << pos_x << std::endl;
            element->dirtyContent = false;
        }

        switch (element->alignment) {
            case ALIGN_L:
                element->beginX = pos_x;  // Ya establecido en línea 557
                break;
            case ALIGN_C:
                // Para centrado: calcular posición después de conocer el ancho
                element->beginX = (cur_mon->width - element->width) / 2;
                pos_x = element->beginX;
                break;
            case ALIGN_R:
                // Para derecha: el elemento va al final
                element->beginX = cur_mon->width - element->width;
                pos_x = element->beginX;
                break;
        }
        std::cout << "Entre y el valor es : " << element->beginX << " y " << element->beginX + element->width << std::endl;

        uint32_t ucs = 0;
        pos_x = element->beginX;
        for (int i = 0; i < element->ucsContentLen; i++) {
            ucs = element->ucsContent[i];
            cur_font = select_drawable_font(ucs);
            //if (!cur_font)
                //continue;  // Si ninguna fuente tiene el glifo, omitir carácter

            // === CONFIGURACIÓN DE FUENTE EN CONTEXTO GRÁFICO ===
            if(cur_font->ptr) {  // Solo para fuentes XCB (no Xft)
                xcb_change_gc(c, gc[GC_DRAW] , XCB_GC_FONT,
                    (const uint32_t []) {
                        cur_font->ptr
                    }
                );
            }

            // === DIBUJAR CARÁCTER ===
            draw_char(cur_mon, cur_font, pos_x, align, ucs);
            pos_x += element->ucsContentCharWidths[i];

            // === ACTUALIZAR POSICIÓN Y ÁREAS ===
            area_shift(cur_mon->window, align, element->ucsContentCharWidths[i]);  // Ajusta áreas clickeables
            //std::cout << "ucs: " << ucs << ", w: " << with_char << std::endl;
        }

    }
    // === LIMPIEZA FINAL ===
    XftDrawDestroy (xft_draw);  // Libera el drawable XFT
}

void
font_load (const char *pattern)
{
    fprintf(stderr, "[lemonbar] font_load: trying to load '%s' (font_count=%d)\n", pattern ? pattern : "(null)", font_count);
    if (font_count >= MAX_FONT_COUNT) {
        fprintf(stderr, "Max font count reached. Could not load font \"%s\"\n", pattern);
        return;
    }

    /* Ensure we have an X connection before attempting XCB/Xft calls. This
     * allows callers to load fonts before the full initialization sequence
     * (useful for in-process use) while avoiding dereferencing a NULL
     * xcb_connection. */
    if (!dpy || !c) {
        fprintf(stderr, "[lemonbar] font_load: no X connection, calling xconn()\n");
        xconn();
    }

    fprintf(stderr, "[lemonbar] font_load: X connection present (dpy=%p c=%p)\n", (void*)dpy, (void*)c);
    xcb_query_font_cookie_t queryreq;
    xcb_query_font_reply_t *font_info;
    xcb_void_cookie_t cookie;
    xcb_font_t font;

    font = xcb_generate_id(c);

    font_t *ret = static_cast<font_t *>(calloc(1, sizeof(font_t)));

    if (!ret)
        return;

    cookie = xcb_open_font_checked(c, font, strlen(pattern), pattern);
    if (!xcb_request_check (c, cookie)) {
        queryreq = xcb_query_font(c, font);
        font_info = xcb_query_font_reply(c, queryreq, NULL);

        ret->xft_ft = NULL;
        ret->ptr = font;
        ret->descent = font_info->font_descent;
        ret->height = font_info->font_ascent + font_info->font_descent;
        ret->width = font_info->max_bounds.character_width;
        ret->char_max = font_info->max_byte1 << 8 | font_info->max_char_or_byte2;
        ret->char_min = font_info->min_byte1 << 8 | font_info->min_char_or_byte2;
        // Copy over the width lut as it's part of font_info
        int lut_size = sizeof(xcb_charinfo_t) * xcb_query_font_char_infos_length(font_info);
        if (lut_size) {
            ret->width_lut = static_cast<xcb_charinfo_t *>(malloc(lut_size));
            memcpy(ret->width_lut, xcb_query_font_char_infos(font_info), lut_size);
        }
        free(font_info);
    } else if ((ret->xft_ft = XftFontOpenName (dpy, scr_nbr, pattern))) {
        ret->ptr = 0;
        ret->ascent = ret->xft_ft->ascent;
        ret->descent = ret->xft_ft->descent;
        ret->height = ret->ascent + ret->descent;
    } else {
        fprintf(stderr, "Could not load font %s\n", pattern);
        free(ret);
        return;
    }

    font_list[font_count++] = ret;
    fprintf(stderr, "[lemonbar] font_load: loaded '%s' into slot %d\n", pattern, font_count-1);
}


void add_y_offset(int offset) {
    if (offset_y_count >= MAX_FONT_COUNT) {
        fprintf(stderr, "Max offset count reached. Could not set offset \"%d\"\n", offset);
        return;
    }

    offsets_y[offset_y_count] = offset;
    if (offset_y_count == 0) {
        for (int i = 1; i < MAX_FONT_COUNT; ++i) {
            offsets_y[i] = offsets_y[0];
        }
    }
    ++offset_y_count;
}


void lemonbar_feed(std::vector<BarElement*> *elements) {
    parseBarElement(elements);

    // Copy pixmap to windows
    for (monitor_t *mon = monhead; mon; mon = mon->next) {
        xcb_copy_area(c, mon->pixmap, mon->window, gc[GC_DRAW], 0, 0, 0, 0, mon->width, bh);
    }
    xcb_flush(c);
    fprintf(stderr, "[lemonbar] lemonbar_feed: flush complete\n");
}

int lemonbar_get_xcb_fd(void) {
    if (!c) return -1;
    return xcb_get_file_descriptor(c);
}

void processXEvents(void) {
    fprintf(stderr, "[lemonbar] lemonbar_process_xevents: polling events\n");
    xcb_generic_event_t *ev;
    bool redraw = false;

    while ((ev = xcb_poll_for_event(c))) {
        xcb_expose_event_t *expose_ev = (xcb_expose_event_t *)ev;
        switch (ev->response_type & 0x7F) {
            case XCB_EXPOSE:
                fprintf(stderr, "[lemonbar] event: EXPOSE count=%d, processing_expose=%s\n", expose_ev->count, processing_expose ? "true" : "false");
                // Skip EXPOSE events that we generated ourselves to prevent infinite loop
                if (expose_ev->count == 0 && !processing_expose) {
                    redraw = true;
                }
                break;
            case XCB_BUTTON_PRESS: {
                xcb_button_press_event_t *press_ev = (xcb_button_press_event_t *)ev;
                fprintf(stderr, "[lemonbar] event: BUTTON_PRESS win=%u detail=%u x=%u\n", press_ev->event, press_ev->detail, press_ev->event_x);

                for (std::vector<BarElement*>::size_type i = 0; i < elements.size(); i++) {
                    for (std::pair<BarElement::EventType, EventFunction> pair : elements[i]->events) {
                        std::cout << "se ejecuto '" << (const int)press_ev->detail << "' y  debe ser '" << pair.first << "'" << std::endl;
                        if (pair.first != (const int)press_ev->detail)
                            continue;
                        if (
                            elements[i]->window == press_ev->event &&
                            ((const int)press_ev->event_x) >= elements[i]->beginX &&
                            ((const int)press_ev->event_x) < (elements[i]->beginX + elements[i]->width)
                        ) {
                            pair.second();
                        }
                    }
                }
                break;
            }
        }
        free(ev);
    }

    if (redraw) {
        fprintf(stderr, "[lemonbar] lemonbar_process_xevents: redraw requested\n");
        // Set flag to prevent infinite EXPOSE loop when we redraw
        processing_expose = true;
        for (monitor_t *mon = monhead; mon; mon = mon->next) {
            xcb_copy_area(c, mon->pixmap, mon->window, gc[GC_DRAW], 0, 0, 0, 0, mon->width, bh);
        }
        xcb_flush(c);
        // Clear the flag after flush is complete
        processing_expose = false;
    }
}


enum {
    NET_WM_WINDOW_TYPE,
    NET_WM_WINDOW_TYPE_DOCK,
    NET_WM_DESKTOP,
    NET_WM_STRUT_PARTIAL,
    NET_WM_STRUT,
    NET_WM_STATE,
    NET_WM_STATE_STICKY,
    NET_WM_STATE_ABOVE,
};

void
set_ewmh_atoms (void)
{
    const char *atom_names[] = {
        "_NET_WM_WINDOW_TYPE",
        "_NET_WM_WINDOW_TYPE_DOCK",
        "_NET_WM_DESKTOP",
        "_NET_WM_STRUT_PARTIAL",
        "_NET_WM_STRUT",
        "_NET_WM_STATE",
        // Leave those at the end since are batch-set
        "_NET_WM_STATE_STICKY",
        "_NET_WM_STATE_ABOVE",
    };
    const int atoms = sizeof(atom_names)/sizeof(char *);
    xcb_intern_atom_cookie_t atom_cookie[atoms];
    xcb_atom_t atom_list[atoms];
    xcb_intern_atom_reply_t *atom_reply;

    // As suggested fetch all the cookies first (yum!) and then retrieve the
    // atoms to exploit the async'ness
    for (int i = 0; i < atoms; i++)
        atom_cookie[i] = xcb_intern_atom(c, 0, strlen(atom_names[i]), atom_names[i]);

    for (int i = 0; i < atoms; i++) {
        atom_reply = xcb_intern_atom_reply(c, atom_cookie[i], NULL);
        if (!atom_reply)
            return;
        atom_list[i] = atom_reply->atom;
        free(atom_reply);
    }

    // Prepare the strut array
    for (monitor_t *mon = monhead; mon; mon = mon->next) {
        int strut[12] = {0};
        if (topbar) {
            strut[2] = bh;
            strut[8] = mon->x;
            strut[9] = mon->x + mon->width;
        } else {
            strut[3]  = bh;
            strut[10] = mon->x;
            strut[11] = mon->x + mon->width;
        }

        xcb_change_property(c, XCB_PROP_MODE_REPLACE, mon->window, atom_list[NET_WM_WINDOW_TYPE], XCB_ATOM_ATOM, 32, 1, &atom_list[NET_WM_WINDOW_TYPE_DOCK]);
        xcb_change_property(c, XCB_PROP_MODE_APPEND,  mon->window, atom_list[NET_WM_STATE], XCB_ATOM_ATOM, 32, 2, &atom_list[NET_WM_STATE_STICKY]);
        xcb_change_property(c, XCB_PROP_MODE_REPLACE, mon->window, atom_list[NET_WM_DESKTOP], XCB_ATOM_CARDINAL, 32, 1, (const uint32_t []) {
            0u - 1u
        } );
        xcb_change_property(c, XCB_PROP_MODE_REPLACE, mon->window, atom_list[NET_WM_STRUT_PARTIAL], XCB_ATOM_CARDINAL, 32, 12, strut);
        xcb_change_property(c, XCB_PROP_MODE_REPLACE, mon->window, atom_list[NET_WM_STRUT], XCB_ATOM_CARDINAL, 32, 4, strut);
        xcb_change_property(c, XCB_PROP_MODE_REPLACE, mon->window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, 3, "bar");
        xcb_change_property(c, XCB_PROP_MODE_REPLACE, mon->window, XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 8, 12, "lemonbar\0Bar");
    }
}

monitor_t *
monitor_new (int x, int y, int width, int height)
{
    monitor_t *ret;

    ret = static_cast<monitor_t *>(calloc(1, sizeof(monitor_t)));
    if (!ret) {
        fprintf(stderr, "Failed to allocate new monitor\n");
        exit(EXIT_FAILURE);
    }

    ret->x = x;
    ret->y = (topbar ? by : height - bh - by) + y;
    ret->width = width;
    ret->next = ret->prev = NULL;
    ret->window = xcb_generate_id(c);
    int depth = (visual == scr->root_visual) ? XCB_COPY_FROM_PARENT : 32;
    xcb_create_window(c, depth, ret->window, scr->root,
                      ret->x, ret->y, width, bh, 0,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT, visual,
                      XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL | XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK | XCB_CW_COLORMAP,
    (const uint32_t []) {
        backgroundColor.v, backgroundColor.v, dock, XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS, colormap
    });

    /* Force full opacity via _NET_WM_WINDOW_OPACITY to work around compositors */
    {
        xcb_intern_atom_cookie_t opa_cookie = xcb_intern_atom(c, 0, strlen("_NET_WM_WINDOW_OPACITY"), "_NET_WM_WINDOW_OPACITY");
        xcb_intern_atom_reply_t *opa_reply = xcb_intern_atom_reply(c, opa_cookie, NULL);
        if (opa_reply) {
            xcb_atom_t opa_atom = opa_reply->atom;
            uint32_t opacity_val = 0xFFFFFFFFu;
            xcb_change_property(c, XCB_PROP_MODE_REPLACE, ret->window, opa_atom, XCB_ATOM_CARDINAL, 32, 1, &opacity_val);
            free(opa_reply);
        }
    }

    ret->pixmap = xcb_generate_id(c);
    xcb_create_pixmap(c, depth, ret->pixmap, ret->window, width, bh);

    return ret;
}

void
monitor_add (monitor_t *mon)
{
    if (!monhead) {
        monhead = mon;
    } else if (!montail) {
        montail = mon;
        monhead->next = mon;
        mon->prev = monhead;
    } else {
        mon->prev = montail;
        montail->next = mon;
        montail = montail->next;
    }
}

int
static rect_sort_cb (const void *p1, const void *p2)
{
    const xcb_rectangle_t *r1 = (xcb_rectangle_t *)p1;
    const xcb_rectangle_t *r2 = (xcb_rectangle_t *)p2;

    if (r1->x < r2->x || r1->y + r1->height <= r2->y)
    {
        return -1;
    }

    if (r1->x > r2->x || r1->y + r1->height > r2->y)
    {
        return 1;
    }

    return 0;
}

void
monitor_create_chain (xcb_rectangle_t *rects, const int num)
{
    int i;
    int width = 0, height = 0;
    int left = bx;

    // Sort before use
    qsort(rects, num, sizeof(xcb_rectangle_t), rect_sort_cb);

    for (i = 0; i < num; i++) {
        int h = rects[i].y + rects[i].height;
        // Accumulated width of all monitors
        width += rects[i].width;
        // Get height of screen from y_offset + height of lowest monitor
        if (h >= height)
            height = h;
    }

    if (bw < 0)
        bw = width - bx;

    // Use the first font height as all the font heights have been set to the biggest of the set
    if (bh < 0 || bh > height)
        bh = font_list[0]->height + bu + 2;

    // Check the geometry
    if (bx + bw > width || by + bh > height) {
        fprintf(stderr, "The geometry specified doesn't fit the screen!\n");
        exit(EXIT_FAILURE);
    }

    // Left is a positive number or zero therefore monitors with zero width are excluded
    width = bw;
    for (i = 0; i < num; i++) {
        if (rects[i].y + rects[i].height < by)
            continue;
        if (rects[i].width > left) {
            monitor_t *mon = monitor_new(
                                 rects[i].x + left,
                                 rects[i].y,
                                 min(width, rects[i].width - left),
                                 rects[i].height);

            if (!mon)
                break;

            monitor_add(mon);

            width -= rects[i].width - left;
            // No need to check for other monitors
            if (width <= 0)
                break;
        }

        left -= rects[i].width;

        if (left < 0)
            left = 0;
    }
}

void
get_randr_monitors (void)
{
    xcb_randr_get_screen_resources_current_reply_t *rres_reply;
    xcb_randr_output_t *outputs;
    int i, j, num, valid = 0;

    rres_reply = xcb_randr_get_screen_resources_current_reply(c,
                 xcb_randr_get_screen_resources_current(c, scr->root), NULL);

    if (!rres_reply) {
        fprintf(stderr, "Failed to get current randr screen resources\n");
        return;
    }

    num = xcb_randr_get_screen_resources_current_outputs_length(rres_reply);
    outputs = xcb_randr_get_screen_resources_current_outputs(rres_reply);


    // There should be at least one output
    if (num < 1) {
        free(rres_reply);
        return;
    }

    xcb_rectangle_t rects[num];

    // Get all outputs
    for (i = 0; i < num; i++) {
        xcb_randr_get_output_info_reply_t *oi_reply;
        xcb_randr_get_crtc_info_reply_t *ci_reply;

        oi_reply = xcb_randr_get_output_info_reply(c, xcb_randr_get_output_info(c, outputs[i], XCB_CURRENT_TIME), NULL);

        // Output disconnected or not attached to any CRTC ?
        if (!oi_reply || oi_reply->crtc == XCB_NONE || oi_reply->connection != XCB_RANDR_CONNECTION_CONNECTED) {
            free(oi_reply);
            rects[i].width = 0;
            continue;
        }

        ci_reply = xcb_randr_get_crtc_info_reply(c,
                   xcb_randr_get_crtc_info(c, oi_reply->crtc, XCB_CURRENT_TIME), NULL);

        free(oi_reply);

        if (!ci_reply) {
            fprintf(stderr, "Failed to get RandR ctrc info\n");
            free(rres_reply);
            return;
        }

        // There's no need to handle rotated screens here (see #69)
        rects[i] = (xcb_rectangle_t){ ci_reply->x, ci_reply->y, ci_reply->width, ci_reply->height };

        free(ci_reply);

        valid++;
    }

    free(rres_reply);

    // Check for clones and inactive outputs
    for (i = 0; i < num; i++) {
        if (rects[i].width == 0)
            continue;

        for (j = 0; j < num; j++) {
            // Does I contain J ?

            if (i != j && rects[j].width) {
                if (rects[j].x >= rects[i].x && rects[j].x + rects[j].width <= rects[i].x + rects[i].width &&
                        rects[j].y >= rects[i].y && rects[j].y + rects[j].height <= rects[i].y + rects[i].height) {
                    rects[j].width = 0;
                    valid--;
                }
            }
        }
    }

    if (valid < 1) {
        fprintf(stderr, "No usable RandR output found\n");
        return;
    }

    xcb_rectangle_t r[valid];

    for (i = j = 0; i < num && j < valid; i++)
        if (rects[i].width != 0)
            r[j++] = rects[i];

    monitor_create_chain(r, valid);
}

#ifdef WITH_XINERAMA
void
get_xinerama_monitors (void)
{
    xcb_xinerama_query_screens_reply_t *xqs_reply;
    xcb_xinerama_screen_info_iterator_t iter;
    int screens;

    xqs_reply = xcb_xinerama_query_screens_reply(c,
                xcb_xinerama_query_screens_unchecked(c), NULL);

    iter = xcb_xinerama_query_screens_screen_info_iterator(xqs_reply);
    screens = iter.rem;

    xcb_rectangle_t rects[screens];

    // Fetch all the screens first
    for (int i = 0; iter.rem; i++) {
        rects[i].x = iter.data->x_org;
        rects[i].y = iter.data->y_org;
        rects[i].width = iter.data->width;
        rects[i].height = iter.data->height;
        xcb_xinerama_screen_info_next(&iter);
    }

    free(xqs_reply);

    monitor_create_chain(rects, screens);
}
#endif

xcb_visualid_t
get_visual (void)
{

    XVisualInfo xv;
    xv.depth = 32;
    int result = 0;
    XVisualInfo* result_ptr = NULL;
    result_ptr = XGetVisualInfo(dpy, VisualDepthMask, &xv, &result);

    if (result > 0) {
        visual_ptr = result_ptr->visual;
        return result_ptr->visualid;
    }

    //Fallback
    visual_ptr = DefaultVisual(dpy, scr_nbr);
    return scr->root_visual;
}

// Parse an X-styled geometry string, we don't support signed offsets though.
bool
parse_geometry_string (char *str, int *tmp)
{
    char *p = str;
    int i = 0, j;

    if (!str || !str[0])
        return false;

    // The leading = is optional
    if (*p == '=')
        p++;

    while (*p) {
        // A geometry string has only 4 fields
        if (i >= 4) {
            fprintf(stderr, "Invalid geometry specified\n");
            return false;
        }
        // Move on if we encounter a 'x' or '+'
        if (*p == 'x') {
            if (i > 0) // The 'x' must precede '+'
                break;
            i++; p++; continue;
        }
        if (*p == '+') {
            if (i < 1) // Stray '+', skip the first two fields
                i = 2;
            else
                i++;
            p++; continue;
        }
        // A digit must follow
        if (!isdigit(*p)) {
            fprintf(stderr, "Invalid geometry specified\n");
            return false;
        }
        // Try to parse the number
        errno = 0;
        j = strtoul(p, &p, 10);
        if (errno) {
            fprintf(stderr, "Invalid geometry specified\n");
            return false;
        }
        tmp[i] = j;
    }

    return true;
}

void
xconn (void)
{
    fprintf(stderr, "[lemonbar] xconn: entering\n");
    if ((dpy = XOpenDisplay(0)) == NULL) {
        fprintf (stderr, "Couldnt open display\n");
    }
    fprintf(stderr, "[lemonbar] xconn: XOpenDisplay returned %p\n", (void*)dpy);

    if ((c = XGetXCBConnection(dpy)) == NULL) {
        fprintf (stderr, "Couldnt connect to X\n");
        exit (EXIT_FAILURE);
    }
    fprintf(stderr, "[lemonbar] xconn: XGetXCBConnection returned %p\n", (void*)c);

	XSetEventQueueOwner(dpy, XCBOwnsEventQueue);
    fprintf(stderr, "[lemonbar] xconn: set event queue owner\n");

    if (xcb_connection_has_error(c)) {
        fprintf(stderr, "Couldn't connect to X\n");
        exit(EXIT_FAILURE);
    }
    fprintf(stderr, "[lemonbar] xconn: xcb connection ok\n");

    /* Grab infos from the first screen */
    scr = xcb_setup_roots_iterator(xcb_get_setup(c)).data;
    fprintf(stderr, "[lemonbar] xconn: got screen data\n");

    /* Try to get a RGBA visual and build the colormap for that */
	visual = get_visual();
    colormap = xcb_generate_id(c);
    xcb_create_colormap(c, XCB_COLORMAP_ALLOC_NONE, colormap, scr->root, visual);
    fprintf(stderr, "[lemonbar] xconn: created colormap %u visual %u\n", colormap, visual);
}

void
init (char *wm_name, char *wm_instance)
{
    // Try to load a default font
    if (!font_count)
        font_load("fixed");

    // We tried and failed hard, there's something wrong
    if (!font_count)
        exit(EXIT_FAILURE);

    // To make the alignment uniform, find maximum height
    int maxh = font_list[0]->height;
    for (int i = 1; i < font_count; i++)
        maxh = max(maxh, font_list[i]->height);

    // Set maximum height to all fonts
    for (int i = 0; i < font_count; i++)
        font_list[i]->height = maxh;

    // Generate a list of screens
    const xcb_query_extension_reply_t *qe_reply;

    // Initialize monitor list head and tail
    monhead = montail = NULL;

    // Check if RandR is present
    qe_reply = xcb_get_extension_data(c, &xcb_randr_id);

    if (qe_reply && qe_reply->present) {
        get_randr_monitors();
    }
#if WITH_XINERAMA
    else {
        qe_reply = xcb_get_extension_data(c, &xcb_xinerama_id);

        // Check if Xinerama extension is present and active
        if (qe_reply && qe_reply->present) {
            xcb_xinerama_is_active_reply_t *xia_reply;
            xia_reply = xcb_xinerama_is_active_reply(c, xcb_xinerama_is_active(c), NULL);

            if (xia_reply && xia_reply->state)
                get_xinerama_monitors();

            free(xia_reply);
        }
    }
#endif

    if (!monhead) {
        // If I fits I sits
        if (bw < 0)
            bw = scr->width_in_pixels - bx;

        // Adjust the height
        if (bh < 0 || bh > scr->height_in_pixels)
            bh = maxh + bu + 2;

        // Check the geometry
        if (bx + bw > scr->width_in_pixels || by + bh > scr->height_in_pixels) {
            fprintf(stderr, "The geometry specified doesn't fit the screen!\n");
            exit(EXIT_FAILURE);
        }

        // If no RandR outputs or Xinerama screens, fall back to using whole screen
        monhead = monitor_new(0, 0, bw, scr->height_in_pixels);
    }

    if (!monhead)
        exit(EXIT_FAILURE);

    // For WM that support EWMH atoms
    set_ewmh_atoms();

    // Create the gc for drawing
    gc[GC_DRAW] = xcb_generate_id(c);
    xcb_create_gc(c, gc[GC_DRAW], monhead->pixmap, XCB_GC_FOREGROUND, (const uint32_t []){ foregroundColor.v });

    gc[GC_CLEAR] = xcb_generate_id(c);
    xcb_create_gc(c, gc[GC_CLEAR], monhead->pixmap, XCB_GC_FOREGROUND, (const uint32_t []){ backgroundColor.v });

    gc[GC_ATTR] = xcb_generate_id(c);
    xcb_create_gc(c, gc[GC_ATTR], monhead->pixmap, XCB_GC_FOREGROUND, (const uint32_t []){ underlineColor.v });

    // Make the bar visible and clear the pixmap
    for (monitor_t *mon = monhead; mon; mon = mon->next) {
        fill_rect(mon->pixmap, gc[GC_CLEAR], 0, 0, mon->width, bh);
        xcb_map_window(c, mon->window);

        // Make sure that the window really gets in the place it's supposed to be
        // Some WM such as Openbox need this
        xcb_configure_window(c, mon->window, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, (const uint32_t []){ (uint32_t)mon->x, (uint32_t)mon->y });

        // Set the WM_NAME atom to the user specified value
        if (wm_name)
            xcb_change_property(c, XCB_PROP_MODE_REPLACE, mon->window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8 ,strlen(wm_name), wm_name);

        // set the WM_CLASS atom instance to the executable name
        if (wm_instance) {
            char *wm_class;
            int wm_class_offset, wm_class_len;

            // WM_CLASS is nullbyte seperated: wm_instance + "\0Bar\0"
            wm_class_offset = strlen(wm_instance) + 1;
            wm_class_len = wm_class_offset + 4;

            wm_class = static_cast<char *>(calloc(1, wm_class_len + 1));
            strcpy(wm_class, wm_instance);
            strcpy(wm_class+wm_class_offset, "Bar");

            xcb_change_property(c, XCB_PROP_MODE_REPLACE, mon->window, XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 8, wm_class_len, wm_class);

            free(wm_class);
        }
    }

    char color[] = "#ffffff";
    uint32_t nfgc = foregroundColor.v & 0x00ffffff;
    snprintf(color, sizeof(color), "#%06X", nfgc);

    if (!XftColorAllocName (dpy, visual_ptr, colormap, color, &sel_fg)) {
        fprintf(stderr, "Couldn't allocate xft font color '%s'\n", color);
    }
    xcb_flush(c);
}

char*
strip_path(char *path)
{
    char *slash;

    if (path == NULL || *path == '\0')
        return strdup("lemonbar");

    slash = strrchr(path, '/');
    if (slash != NULL)
        return strndup(slash + 1, 31);

    return strndup(path, 31);
}

static void
sighandle (int signal)
{
    if (signal == SIGINT || signal == SIGTERM)
        exit(EXIT_SUCCESS);
}

    ~Bar() {
        for (int i = 0; font_list[i]; i++) {
            if (font_list[i]->xft_ft) {
                XftFontClose (dpy, font_list[i]->xft_ft);
            }
            else {
                xcb_close_font(c, font_list[i]->ptr);
                free(font_list[i]->width_lut);
            }
            free(font_list[i]);
        }

        while (monhead) {
            monitor_t *next = monhead->next;
            xcb_destroy_window(c, monhead->window);
            xcb_free_pixmap(c, monhead->pixmap);
            free(monhead);
            monhead = next;
        }

        XftColorFree(dpy, visual_ptr, colormap, &sel_fg);

        if (gc[GC_DRAW])
            xcb_free_gc(c, gc[GC_DRAW]);
        if (gc[GC_CLEAR])
            xcb_free_gc(c, gc[GC_CLEAR]);
        if (gc[GC_ATTR])
            xcb_free_gc(c, gc[GC_ATTR]);
        if (c)
            xcb_disconnect(c);
    }
};
#endif /* LEMONBAR_LIB */
