
#ifndef COLOR_H
#define COLOR_H

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

  Color() : v(0x00000000u) {}
  Color(uint32_t val) : v(val) {}

  Color(uint8_t _b, uint8_t _g, uint8_t _r, uint8_t _a = 255) : b(_b), g(_g), r(_r), a(_a) {}

  bool operator==(const Color& other) const {
    return v == other.v;
  }
  bool operator!=(const Color& other) const {
    return !(*this == other);
  }

  //static constexpr const int test = 0xffffff;
  static constexpr const int RED         = 0xFFFE6B6Bu;
  static constexpr const int PURPLE      = 0xFFDFA9FEu;
  static constexpr const int GREEN       = 0xFF8FED8Fu;
  static constexpr const int ORANGE      = 0xFFFEA400u;
  static constexpr const int BLUE        = 0xFF504DFFu;
  static constexpr const int GRAY        = 0xFF666666u;
  static constexpr const int DEEP_VIOLET = 0xFF1A0B2Eu;

};

#endif // COLOR_H
