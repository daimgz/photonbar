#ifndef FONT_MANAGER_H
#define FONT_MANAGER_H

#include "bar_types.h"
#include <string>
#include <vector>

#define MAX_FONT_COUNT 5
#define MAX_WIDTHS_CACHE (1 << 16)

class FontManager {
public:
    FontManager();
    ~FontManager();

    void loadFont(const char* pattern);
    void addYOffset(int offset);
    bool hasGlyph(font_t* font, uint32_t c);
    font_t* selectFontForCharacter(uint32_t c);
    int getCharacterWidth(uint32_t ucs, font_t* font);
    UTF8Result decodeUtf8(const char* input);
    int getFontCount() const { return fontCount; }
    font_t* getFont(int index) const { return (index >= 0 && index < fontCount) ? fontList[index] : nullptr; }
    int getFontHeight() const;
    int getSpaceWidth();

    int calcXftCharWidthSlot(uint32_t ch) {
        int slot = ch % MAX_WIDTHS_CACHE;
        while (xftChar[slot] != 0 && xftChar[slot] != (wchar_t)ch) {
            slot = (slot + 1) % MAX_WIDTHS_CACHE;
        }
        return slot;
    }

    int calcXftCharWidth(uint32_t ch, font_t* curFont);

    void setDisplay(void* dpy) { this->dpy = dpy; }
    void setConnection(void* c) { this->c = c; }

    font_t* fontList[MAX_FONT_COUNT];
    int fontCount;
    int fontIndex;
    int offsetsY[MAX_FONT_COUNT];
    int offsetYCount;
    int offsetYIndex;

    wchar_t xftChar[MAX_WIDTHS_CACHE];
    char xftWidth[MAX_WIDTHS_CACHE];
    uint8_t fontIndexCache[MAX_WIDTHS_CACHE];
    bool fontIndexCacheValid[MAX_WIDTHS_CACHE];
    int cachedSpaceWidth;
    void* cachedSpaceFont;

private:
    void* dpy;
    void* c;

    bool fontHasGlyphInternal(font_t* font, uint32_t c);
};

#endif