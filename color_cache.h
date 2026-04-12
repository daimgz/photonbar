#ifndef COLOR_CACHE_H
#define COLOR_CACHE_H

#include "color.h"

namespace ColorCache {
    inline Color green() { return Color(0, 255, 0, 255); }
    inline Color orange() { return Color(255, 165, 0, 255); }
    inline Color red() { return Color(255, 0, 0, 255); }
    inline Color purple() { return Color(224, 170, 255, 255); }
    inline Color lightRed() { return Color(255, 107, 107, 255); }
    inline Color blue() { return Color(107, 182, 255, 255); }
    inline Color gray() { return Color(102, 102, 102, 255); }
    inline Color lightGreen() { return Color(144, 238, 144, 255); }
}

#endif