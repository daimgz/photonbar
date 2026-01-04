
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
class Color {

public:
    union {
        struct {
            uint8_t b, g, r, a;
        };
        uint32_t v;
    };

    // 1. Constructor por defecto (necesario)
    Color() : v(0) {}

    // 2. Constructor para un solo valor entero: rgba_t col = 0xFF00AA;
    Color(uint32_t val) : v(val) {}

    // 3. Constructor para componentes individuales: rgba_t col = {b, g, r, a};
    Color(uint8_t _b, uint8_t _g, uint8_t _r, uint8_t _a = 255)
        : b(_b), g(_g), r(_r), a(_a) {}
    bool operator==(const Color& other) const {
        return v == other.v;
    }
    bool operator!=(const Color& other) const {
        return !(*this == other);
    }
    static Color parse_color (const char *str, char **end, const Color def)
    {
        int string_len;
        char *ep;

        if (!str)
            return def;

        // Reset
        if (str[0] == '-') {
            if (end)
                *end = (char *)str + 1;

            return def;
        }

        // Hex representation
        if (str[0] != '#') {
            if (end)
                *end = (char *)str;

            fprintf(stderr, "Invalid color specified\n");
            return def;
        }

        errno = 0;
        //rgba_t tmp = (rgba_t)(uint32_t)strtoul(str + 1, &ep, 16);
        Color tmp((uint32_t)strtoul(str + 1, &ep, 16));

        if (end)
            *end = ep;

        // Some error checking is definitely good
        if (errno) {
            fprintf(stderr, "Invalid color specified\n");
            return def;
        }

        string_len = ep - (str + 1);

        switch (string_len) {
            case 3:
                // Expand the #rgb format into #rrggbb (aa is set to 0xff)
                tmp.v = (tmp.v & 0xf00) * 0x1100
                      | (tmp.v & 0x0f0) * 0x0110
                      | (tmp.v & 0x00f) * 0x0011;
            case 6:
                // If the code is in #rrggbb form then assume it's opaque
                tmp.a = 255;
                break;
            case 7:
            case 8:
                // Colors in #aarrggbb format, those need no adjustments
                break;
            default:
                fprintf(stderr, "Invalid color specified\n");
                return def;
        }

        // Force opacity: ignore any alpha specified and make it fully opaque
        tmp.a = 255;

        // Premultiply the alpha in
        if (tmp.a) {
            // The components are clamped automagically as the rgba_t is made of uint8_t
            return Color(
                static_cast<uint8_t>((tmp.b * tmp.a) / 255),
                static_cast<uint8_t>((tmp.g * tmp.a) / 255),
                static_cast<uint8_t>((tmp.r * tmp.a) / 255),
                static_cast<uint8_t>(tmp.a)
            );
        }

        return Color(0U);
    }
};
