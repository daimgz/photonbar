#ifndef BAR_TYPES_H
#define BAR_TYPES_H

#include <stdint.h>
#include <xcb/xcb.h>

#ifndef FT_UInt
typedef unsigned int FT_UInt;
#endif

typedef struct font_t {
    xcb_font_t ptr;
    xcb_charinfo_t *width_lut;
    void *xft_ft; // XftFont* - forward declaration
    int ascent;
    int descent, height, width;
    uint32_t charMax;
    uint32_t charMin;
} font_t;

typedef struct monitor_t {
    int x, y, width;
    xcb_window_t window;
    xcb_pixmap_t pixmap;
    struct monitor_t *prev, *next;
} monitor_t;

struct UTF8Result {
    uint32_t ucs;
    int bytesConsumed;
};

enum {
    ATTR_OVERL = (1<<0),
    ATTR_UNDERL = (1<<1),
};

enum {
    ALIGN_L = 0,
    ALIGN_C,
    ALIGN_R
};

enum {
    GC_DRAW = 0,
    GC_CLEAR,
    GC_ATTR,
    GC_MAX
};

#define MAX_FONT_COUNT 5
#define MAX_WIDTHS (1 << 16)

#endif