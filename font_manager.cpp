#include "font_manager.h"

#include <X11/Xft/Xft.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include <cstring>
#include <cstdlib>

FontManager::FontManager() : fontCount(0), fontIndex(-1), offsetYCount(0), offsetYIndex(0), dpy(nullptr), c(nullptr), cachedSpaceWidth(0), cachedSpaceFont(nullptr) {
    memset(fontList, 0, sizeof(fontList));
    memset(offsetsY, 0, sizeof(offsetsY));
    memset(xftChar, 0, sizeof(xftChar));
    memset(xftWidth, 0, sizeof(xftWidth));
    memset(fontIndexCache, 0, sizeof(fontIndexCache));
    memset(fontIndexCacheValid, 0, sizeof(fontIndexCacheValid));
}

FontManager::~FontManager() {
    for (int i = 0; fontList[i]; i++) {
        if (fontList[i]->xft_ft) {
            XftFontClose((Display*)dpy, (XftFont*)fontList[i]->xft_ft);
        } else {
            xcb_close_font((xcb_connection_t*)c, fontList[i]->ptr);
            free(fontList[i]->width_lut);
        }
        free(fontList[i]);
    }
}

UTF8Result FontManager::decodeUtf8(const char* input) {
    const uint8_t *utf = (const uint8_t *)input;
    UTF8Result result = {0, 1};

    if (utf[0] < 0x80) {
        result.ucs = utf[0];
        result.bytesConsumed = 1;
    } else if ((utf[0] & 0xe0) == 0xc0) {
        result.ucs = (utf[0] & 0x1f) << 6 | (utf[1] & 0x3f);
        result.bytesConsumed = 2;
    } else if ((utf[0] & 0xf0) == 0xe0) {
        result.ucs = (utf[0] & 0xf) << 12 | (utf[1] & 0x3f) << 6 | (utf[2] & 0x3f);
        result.bytesConsumed = 3;
    } else if ((utf[0] & 0xf8) == 0xf0) {
        result.ucs = (utf[0] & 0x07) << 18 | (utf[1] & 0x3f) << 12 | (utf[2] & 0x3f) << 6 | (utf[3] & 0x3f);
        result.bytesConsumed = 4;
    } else {
        result.ucs = utf[0];
        result.bytesConsumed = 1;
    }

    return result;
}

int FontManager::getCharacterWidth(uint32_t ucs, font_t* font) {
    if (!font) return 0;

    if (font->xft_ft) {
        return calcXftCharWidth(ucs, font);
    } else {
        return (font->width_lut && ucs >= font->charMin && ucs <= font->charMax) ?
            font->width_lut[ucs - font->charMin].character_width :
            font->width;
    }
}

void FontManager::loadFont(const char *pattern) {
    if (fontCount >= MAX_FONT_COUNT) return;

    xcb_connection_t* xc = (xcb_connection_t*)c;
    Display* display = (Display*)dpy;

    xcb_query_font_cookie_t queryreq;
    xcb_query_font_reply_t *font_info;
    xcb_void_cookie_t cookie;
    xcb_font_t font = xcb_generate_id(xc);

    font_t *ret = static_cast<font_t *>(calloc(1, sizeof(font_t)));
    if (!ret) return;

    cookie = xcb_open_font_checked(xc, font, strlen(pattern), pattern);
    if (!xcb_request_check(xc, cookie)) {
        queryreq = xcb_query_font(xc, font);
        font_info = xcb_query_font_reply(xc, queryreq, NULL);

        ret->xft_ft = NULL;
        ret->ptr = font;
        ret->descent = font_info->font_descent;
        ret->height = font_info->font_ascent + font_info->font_descent;
        ret->width = font_info->max_bounds.character_width;
        ret->charMax = font_info->max_byte1 << 8 | font_info->max_char_or_byte2;
        ret->charMin = font_info->min_byte1 << 8 | font_info->min_char_or_byte2;
        int lut_size = sizeof(xcb_charinfo_t) * xcb_query_font_char_infos_length(font_info);
        if (lut_size) {
            ret->width_lut = static_cast<xcb_charinfo_t *>(malloc(lut_size));
            memcpy(ret->width_lut, xcb_query_font_char_infos(font_info), lut_size);
        }
        free(font_info);
    } else if ((ret->xft_ft = (void*)XftFontOpenName(display, 0, pattern))) {
        ret->ptr = 0;
        ret->ascent = ((XftFont*)ret->xft_ft)->ascent;
        ret->descent = ((XftFont*)ret->xft_ft)->descent;
        ret->height = ret->ascent + ret->descent;
    } else {
        free(ret);
        return;
    }

    fontList[fontCount++] = ret;
}

void FontManager::addYOffset(int offset) {
    if (offsetYCount >= MAX_FONT_COUNT) return;
    offsetsY[offsetYCount] = offset;
    if (offsetYCount == 0) {
        for (int i = 1; i < MAX_FONT_COUNT; ++i) offsetsY[i] = offsetsY[0];
    }
    ++offsetYCount;
}

bool FontManager::fontHasGlyphInternal(font_t *font, uint32_t c) {
    Display* d = (Display*)dpy;
    if (font->xft_ft) {
        return XftCharExists(d, (XftFont*)font->xft_ft, (FcChar32)c);
    }
    if (c < font->charMin || c > font->charMax) return false;
    if (font->width_lut && font->width_lut[c - font->charMin].character_width == 0) return false;
    return true;
}

bool FontManager::hasGlyph(font_t* font, uint32_t c) {
    return fontHasGlyphInternal(font, c);
}

font_t* FontManager::selectFontForCharacter(uint32_t c) {
    int slot = c % MAX_WIDTHS_CACHE;
    if (fontIndexCacheValid[slot] && fontIndexCache[slot] < fontCount) {
        int cachedIdx = fontIndexCache[slot];
        if (cachedIdx >= 0 && cachedIdx < fontCount) {
            offsetYIndex = cachedIdx;
            return fontList[cachedIdx];
        }
    }
    if (fontIndex != -1 && fontHasGlyphInternal(fontList[fontIndex - 1], c)) {
        offsetYIndex = fontIndex - 1;
        int cachedSlot = c % MAX_WIDTHS_CACHE;
        fontIndexCache[cachedSlot] = fontIndex - 1;
        fontIndexCacheValid[cachedSlot] = true;
        return fontList[fontIndex - 1];
    }
    for (int i = 0; i < fontCount; i++) {
        if (fontHasGlyphInternal(fontList[i], c)) {
            offsetYIndex = i;
            int cachedSlot = c % MAX_WIDTHS_CACHE;
            fontIndexCache[cachedSlot] = i;
            fontIndexCacheValid[cachedSlot] = true;
            return fontList[i];
        }
    }
    return NULL;
}

int FontManager::getFontHeight() const {
    if (fontCount == 0) return 0;
    int maxh = fontList[0]->height;
    for (int i = 1; i < fontCount; i++) {
        if (fontList[i]->height > maxh) maxh = fontList[i]->height;
    }
    return maxh;
}

int FontManager::calcXftCharWidth(uint32_t ch, font_t* curFont) {
    Display* display = (Display*)dpy;
    int startSlot = calcXftCharWidthSlot(ch);
    int slot = startSlot;
    
    while (xftChar[slot] != 0) {
        if (xftChar[slot] == (wchar_t)ch) {
            return xftWidth[slot];
        }
        slot = (slot + 1) % MAX_WIDTHS_CACHE;
        if (slot == startSlot) return 0;
    }
    
    XGlyphInfo gi;
    XftFont* xftFont = (XftFont*)curFont->xft_ft;
    FT_UInt glyph = XftCharIndex(display, xftFont, (FcChar32)ch);
    FT_UInt glyphVal = glyph;
    XftFontLoadGlyphs(display, xftFont, FcFalse, &glyphVal, 1);
    XftGlyphExtents(display, xftFont, &glyphVal, 1, &gi);
    XftFontUnloadGlyphs(display, xftFont, &glyphVal, 1);
    xftChar[slot] = (wchar_t)ch;
    xftWidth[slot] = (gi.xOff >= gi.width) ? gi.xOff : gi.width;
    return xftWidth[slot];
}

int FontManager::getSpaceWidth() {
    font_t* spaceFont = selectFontForCharacter(' ');
    if (cachedSpaceFont != spaceFont || cachedSpaceWidth == 0) {
        cachedSpaceFont = spaceFont;
        cachedSpaceWidth = getCharacterWidth(' ', spaceFont);
    }
    return cachedSpaceWidth;
}