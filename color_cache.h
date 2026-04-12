#ifndef COLOR_CACHE_H
#define COLOR_CACHE_H

#include "color.h"

namespace ColorCache {
    inline Color green() { return Color(0, 255, 0, 255); }
    inline Color orange() { return Color(0, 165, 255, 255); }
    inline Color red() { return Color(0, 0, 255, 255); }
    inline Color purple() { return Color(255, 170, 224, 255); }
    inline Color lightRed() { return Color(107, 107, 255, 255); }
    inline Color blue() { return Color(255, 182, 107, 255); }
    inline Color gray() { return Color(102, 102, 102, 255); }
    inline Color lightGreen() { return Color(144, 238, 144, 255); }
}

#endif