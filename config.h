#ifndef CONFIG_H
#define CONFIG_H

#define DEBUG 1

#include "color.h"

namespace Config {
  const constexpr uint32_t COLOR_FOREGROUND = Color::PURPLE;
  const constexpr uint32_t COLOR_BACKGROUND = Color::DEEP_VIOLET;
  const constexpr uint32_t COLOR_SEP        = Color::PURPLE;

  const constexpr char* SEPARATION_SIMBOL = " ▏";

  const constexpr char* FONT_TEXT       = "DejaVu Sans Mono:size=16";
  const constexpr char* FONT_ICON       = "Symbols Nerd Font:size=16";
}

#endif
