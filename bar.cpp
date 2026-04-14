#include "bar.h"

#include <X11/Xlib-xcb.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

Bar::Bar(
  const char *name,
  const char *_backgroundColor,
  const char *_foregroundColor,
  const bool topBar,
  const std::vector<std::string> &fonts,
  const std::vector<Module*> &leftModules,
  const std::vector<Module*> &rightModules
) :
  topbar(topBar),
  leftModules(leftModules),
  rightModules(rightModules),
  lastBgColor(0), lastFgColor(0), lastUlColor(0),
  xftDrawInitialized(false), xftDraw(nullptr),
  ownsConnection(true)
{
  memset(lastXftColor, 0, 8);

  modules.insert(modules.end(), leftModules.begin(), leftModules.end());
  modules.insert(modules.end(), rightModules.begin(), rightModules.end());

  defaultBackgroundColor = backgroundColor = Color::parse_color(_backgroundColor, NULL, (Color)0x00000000U);
  defaultForegroundColor = foregroundColor = Color::parse_color(_foregroundColor, NULL, (Color)0x11111111U);
  defaultUnderlineColor = underlineColor = foregroundColor;

  xconn();

  for (const auto& font : fonts) {
    fontManager.loadFont(font.c_str());
  }

  init((char *)name, (char *)name);
  initSeparator();
}

Bar::Bar(
  const char *name,
  const char *_backgroundColor,
  const char *_foregroundColor,
  const bool topBar,
  const std::vector<std::string> &fonts,
  const std::vector<Module*> &leftModules,
  const std::vector<Module*> &rightModules,
  xcb_connection_t* existing_c,
  Display* existing_dpy,
  xcb_visualid_t existing_visual,
  Visual* existing_visualPtr,
  xcb_colormap_t existing_colormap,
  xcb_screen_t* existing_scr,
  FontManager* existing_fontManager
) :
  topbar(topBar),
  leftModules(leftModules),
  rightModules(rightModules),
  dpy(existing_dpy),
  c(existing_c),
  visual(existing_visual),
  visualPtr(existing_visualPtr),
  colormap(existing_colormap),
  scr(existing_scr),
  lastBgColor(0), lastFgColor(0), lastUlColor(0),
  xftDrawInitialized(false), xftDraw(nullptr),
  ownsConnection(false)
{
  memset(lastXftColor, 0, 8);

  modules.insert(modules.end(), leftModules.begin(), leftModules.end());
  modules.insert(modules.end(), rightModules.begin(), rightModules.end());

  defaultBackgroundColor = backgroundColor = Color::parse_color(_backgroundColor, NULL, (Color)0x00000000U);
  defaultForegroundColor = foregroundColor = Color::parse_color(_foregroundColor, NULL, (Color)0x11111111U);
  defaultUnderlineColor = underlineColor = foregroundColor;

  if (existing_fontManager) {
    fontManager = *existing_fontManager;
  } else {
    for (const auto& font : fonts) {
      fontManager.loadFont(font.c_str());
    }
  }

  if (!fontManager.getFontCount()) fontManager.loadFont("fixed");

  int maxh = fontManager.getFontHeight();
  for (int i = 0; i < fontManager.getFontCount(); i++) {
    fontManager.getFont(i)->height = maxh;
  }

  const xcb_query_extension_reply_t *qe_reply;
  monhead = montail = NULL;
  qe_reply = xcb_get_extension_data(c, &xcb_randr_id);

  if (qe_reply && qe_reply->present) {
    getRandrMonitors();
  }

  if (!monhead) {
    if (bw < 0) bw = scr->width_in_pixels - bx;
    if (bh < 0 || bh > scr->height_in_pixels) bh = maxh + bu + 2;
    if (bx + bw > scr->width_in_pixels || by + bh > scr->height_in_pixels) {
      exit(EXIT_FAILURE);
    }
    monhead = monitorNew(0, 0, bw, scr->height_in_pixels);
  }

  setEwmhAtoms();

  gc[GC_DRAW] = xcb_generate_id(c);
  xcb_create_gc(c, gc[GC_DRAW], monhead->pixmap, XCB_GC_FOREGROUND, (const uint32_t []){ foregroundColor.v });
  gc[GC_CLEAR] = xcb_generate_id(c);
  xcb_create_gc(c, gc[GC_CLEAR], monhead->pixmap, XCB_GC_FOREGROUND, (const uint32_t []){ backgroundColor.v });
  gc[GC_ATTR] = xcb_generate_id(c);
  xcb_create_gc(c, gc[GC_ATTR], monhead->pixmap, XCB_GC_FOREGROUND, (const uint32_t []){ underlineColor.v });

  for (monitor_t *mon = monhead; mon; mon = mon->next) {
    fillRect(mon->pixmap, gc[GC_CLEAR], 0, 0, mon->width, bh);
    xcb_map_window(c, mon->window);
    xcb_configure_window(c, mon->window, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, (const uint32_t []){ (uint32_t)mon->x, (uint32_t)mon->y });

    xcb_change_property(c, XCB_PROP_MODE_REPLACE, mon->window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, strlen(name), name);

    char *wm_class;
    int wm_class_offset = strlen(name) + 1;
    int wm_class_len = wm_class_offset + 4;
    wm_class = static_cast<char *>(calloc(1, wm_class_len + 1));
    strcpy(wm_class, name);
    strcpy(wm_class + wm_class_offset, "Bar");
    xcb_change_property(c, XCB_PROP_MODE_REPLACE, mon->window, XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 8, wm_class_len, wm_class);
    free(wm_class);
  }

  initSystemTray();

  char color[] = "#ffffff";
  uint32_t nfgc = foregroundColor.v & 0x00ffffff;
  snprintf(color, sizeof(color), "#%06X", nfgc);
  XftColorAllocName(dpy, visualPtr, colormap, color, &selFg);
  xcb_flush(c);

  initSeparator();
}

Bar::~Bar() {
  if (xftDraw) XftDrawDestroy(xftDraw);

  while (monhead) {
    monitor_t *next = monhead->next;
    xcb_destroy_window(c, monhead->window);
    xcb_free_pixmap(c, monhead->pixmap);
    free(monhead);
    monhead = next;
  }

  XftColorFree(dpy, visualPtr, colormap, &selFg);

  if (gc[GC_DRAW]) xcb_free_gc(c, gc[GC_DRAW]);
  if (gc[GC_CLEAR]) xcb_free_gc(c, gc[GC_CLEAR]);
  if (gc[GC_ATTR]) xcb_free_gc(c, gc[GC_ATTR]);
  if (ownsConnection && c) xcb_disconnect(c);
}

UTF8Result Bar::decodeUtf8Char(const char* input) {
  return fontManager.decodeUtf8(input);
}

int Bar::getUtf8CharWidth(uint32_t ucs, font_t* font) {
  return fontManager.getCharacterWidth(ucs, font);
}

void Bar::setClickHandler(std::function<void(const char *cmd)> cb) {
  clickCb = cb;
}

int Bar::getXcbFd(void) {
  if (!c) return -1;
  return xcb_get_file_descriptor(c);
}

void Bar::processXEvents(void) {
  xcb_generic_event_t *ev;
  bool redraw = false;

  while ((ev = xcb_poll_for_event(c))) {
    xcb_expose_event_t *expose_ev = (xcb_expose_event_t *)ev;
    switch (ev->response_type & 0x7F) {
      case XCB_EXPOSE:
        if (expose_ev->count == 0 && !processingExpose) {
          redraw = true;
        }
        break;
      case XCB_BUTTON_PRESS: {
        xcb_button_press_event_t *press_ev = (xcb_button_press_event_t *)ev;
        bool eventHandled = false;
        for (Module *module : modules) {
          if (eventHandled) break;
          for (BarElement *element : module->getElements()) {
            if (eventHandled) break;
            for (std::pair<BarElement::EventType, EventFunction> pair : element->events) {
              if (pair.first != (const int)press_ev->detail) continue;
              if (module->window == press_ev->event &&
                ((const int)press_ev->event_x) >= element->beginX &&
                ((const int)press_ev->event_x) < (element->beginX + element->width)) {
                pair.second();
                eventHandled = true;
                break;
              }
            }
          }
        }
        break;
      }
      case XCB_CLIENT_MESSAGE: {
        auto *cm = reinterpret_cast<xcb_client_message_event_t *>(ev);
        handleTrayClientMessage(cm);
        break;
      }
    }
    free(ev);
  }

  if (redraw) {
    processingExpose = true;
    for (monitor_t *mon = monhead; mon; mon = mon->next) {
      xcb_copy_area(c, mon->pixmap, mon->window, gc[GC_DRAW], 0, 0, 0, 0, mon->width, bh);
    }
    xcb_flush(c);
    processingExpose = false;
  }
}

void Bar::feed() {
  parseModules();

  for (monitor_t *mon = monhead; mon; mon = mon->next) {
    xcb_copy_area(c, mon->pixmap, mon->window, gc[GC_DRAW], 0, 0, 0, 0, mon->width, bh);
  }
  xcb_flush(c);
}

void Bar::fontLoad(const char *pattern) {
  fontManager.loadFont(pattern);
}

void Bar::addYOffset(int offset) {
  fontManager.addYOffset(offset);
}

bool Bar::fontHasGlyph(font_t *font, const uint32_t c) {
  return fontManager.hasGlyph(font, c);
}

font_t* Bar::selectDrawableFont(const uint32_t c) {
  return fontManager.selectFontForCharacter(c);
}

int Bar::xftCharWidthSlot(uint32_t ch) {
  return fontManager.calcXftCharWidthSlot(ch);
}

int Bar::xftCharWidth(uint32_t ch, font_t* curFont) {
  return fontManager.calcXftCharWidth(ch, curFont);
}

int Bar::drawChar(monitor_t* mon, font_t* curFont, int x, int align, uint32_t ch) {
  int chWidth = fontManager.getCharacterWidth(ch, curFont);
  x = shift(mon, x, align, chWidth);
  int y = bh / 2 + curFont->height / 2 - curFont->descent + fontManager.offsetsY[fontManager.offsetYIndex];

  if (curFont->xft_ft) {
    XftDrawString32(xftDraw, &selFg, (XftFont*)curFont->xft_ft, x, y, (const FcChar32 *)&ch, 1);
  } else {
    if (ch <= 0xFFFF) {
      uint16_t ch16 = (uint16_t)ch;
      ch16 = (ch16 >> 8) | (ch16 << 8);
      xcb_poly_text_16_simple(c, mon->pixmap, gc[GC_DRAW], x, y, 1, &ch16);
    }
  }

  drawLines(mon, x, chWidth);
  return chWidth;
}

int Bar::shift(monitor_t* mon, int x, int align, int chWidth) {
  switch (align) {
    case ALIGN_C:
      xcb_copy_area(c, mon->pixmap, mon->pixmap, gc[GC_DRAW],
                    mon->width / 2 - x / 2, 0,
                    mon->width / 2 - (x + chWidth) / 2, 0,
                    x, bh);
      x = mon->width / 2 - (x + chWidth) / 2 + x;
      break;
    case ALIGN_R:
      xcb_copy_area(c, mon->pixmap, mon->pixmap, gc[GC_DRAW],
                    mon->width - x, 0,
                    mon->width - x - chWidth, 0,
                    x, bh);
      x = mon->width - chWidth;
      break;
  }
  fillRect(mon->pixmap, gc[GC_CLEAR], x, 0, chWidth, bh);
  return x;
}

void Bar::drawLines(monitor_t* mon, int x, int w) {
  if (attrs & ATTR_OVERL)
    fillRect(mon->pixmap, gc[GC_ATTR], x, 0, w, bu);
  if (attrs & ATTR_UNDERL)
    fillRect(mon->pixmap, gc[GC_ATTR], x, bh - bu, w, bu);
}

void Bar::drawShift(monitor_t* mon, int x, int align, int w) {
  x = shift(mon, x, align, w);
  drawLines(mon, x, w);
}

xcb_void_cookie_t Bar::xcb_poly_text_16_simple(xcb_connection_t *c,
                                               xcb_drawable_t drawable, xcb_gcontext_t gc, int16_t x, int16_t y,
                                               uint32_t len, const uint16_t *str) {
  static const xcb_protocol_request_t xcb_req = {5, 0, XCB_POLY_TEXT_16, 1};
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

void Bar::fillRect(xcb_drawable_t d, xcb_gcontext_t _gc, int x, int y, int width, int height) {
  xcb_rectangle_t rect = {static_cast<int16_t>(x), static_cast<int16_t>(y),
    static_cast<uint16_t>(width), static_cast<uint16_t>(height)};
  xcb_poly_fill_rectangle(c, d, _gc, 1, &rect);
}

void Bar::setAttribute(const char modifier, const char attribute) {
  int pos = (int)(strchr("ou", attribute) - "ou");
  if (pos < 0) return;

  switch (modifier) {
    case '+': attrs |= (1u<<pos); break;
    case '-': attrs &=~(1u<<pos); break;
    case '!': attrs ^= (1u<<pos); break;
  }
}

void Bar::updateGc(void) {
  if (!colorsDirty) return;

  if (backgroundColor.v != lastBgColor.v || foregroundColor.v != lastFgColor.v || underlineColor.v != lastUlColor.v) {
    lastBgColor = backgroundColor;
    lastFgColor = foregroundColor;
    lastUlColor = underlineColor;

    xcb_change_gc(c, gc[GC_DRAW], XCB_GC_FOREGROUND, (const uint32_t []){ foregroundColor.v });
    xcb_change_gc(c, gc[GC_CLEAR], XCB_GC_FOREGROUND, (const uint32_t []){ backgroundColor.v });
    xcb_change_gc(c, gc[GC_ATTR], XCB_GC_FOREGROUND, (const uint32_t []){ underlineColor.v });

    char color[8];
    uint32_t nfgc = foregroundColor.v & 0x00ffffff;
    snprintf(color, sizeof(color), "#%06X", nfgc);
    if (memcmp(lastXftColor, color, 7) != 0) {
      memcpy(lastXftColor, color, 7);
      XftColorFree(dpy, visualPtr, colormap, &selFg);
      XftColorAllocName(dpy, visualPtr, colormap, color, &selFg);
    }
  }

  colorsDirty = false;
}

void Bar::markColorsDirty(void) {
  colorsDirty = true;
}

void Bar::initSeparator() {
  const char* sep_string = " ▏";
  const char* p = sep_string;
  int i = 0;
  separator.totalWidth = 0;

  while (*p != '\0' && i < 2) {
    UTF8Result result = fontManager.decodeUtf8(p);
    font_t* f = fontManager.selectFontForCharacter(result.ucs);
    if (!f) result.ucs = '?';

    int w = fontManager.getCharacterWidth(result.ucs, f);
    separator.ucs[i] = result.ucs;
    separator.fonts[i] = f;
    separator.widths[i] = w;
    separator.totalWidth += w;

    p += result.bytesConsumed;
    i++;
  }
}

int Bar::renderSeparatorAt(monitor_t* cur_mon, int current_x) {
  backgroundColor = defaultBackgroundColor;
  foregroundColor = defaultForegroundColor;
  markColorsDirty();
  updateGc();

  int pos_x = current_x;
  font_t* lastFont = nullptr;

  for (int i = 0; i < 2; i++) {
    auto& chFont = separator.fonts[i];
    if (chFont != lastFont) {
      if (chFont->ptr)
        xcb_change_gc(c, gc[GC_DRAW], XCB_GC_FONT, (const uint32_t[]){chFont->ptr});
      lastFont = chFont;
    }
    drawChar(cur_mon, chFont, pos_x, ALIGN_L, separator.ucs[i]);
    pos_x += separator.widths[i];
  }

  return pos_x;
}

void Bar::parseElementContent(BarElement* element) {
  if (!element->dirtyContent) return;

  char *p = element->content;
  uint8_t char_width = 0;
  int total_width = 0;

  for (int i = 0; i < CONTENT_MAX_LEN; i++) {
    if (*p == '\0' || *p == '\n') {
      element->ucsContent[i] = '\0';
      element->ucsContentLen = i;
      break;
    }

    UTF8Result result = fontManager.decodeUtf8(p);
    font_t *curFont = fontManager.selectFontForCharacter(result.ucs);
    if (!curFont) result.ucs = '?';

    char_width = fontManager.getCharacterWidth(result.ucs, curFont);
    element->ucsContent[i] = result.ucs;
    element->ucsContentCharWidths[i] = char_width;
    element->ucsContentFonts[i] = (void*)curFont;
    total_width += char_width;

    p += result.bytesConsumed;
  }

  element->width = total_width;
  element->dirtyContent = false;
}

void Bar::renderElement(BarElement* element, monitor_t* cur_mon) {
  attrs = 0;

  Color oldBgColor = backgroundColor;
  Color oldFgColor = foregroundColor;
  Color oldUlColor = underlineColor;

  if (element->backgroundColor != Color(0x00000000U))
    backgroundColor = element->backgroundColor;
  else
    backgroundColor = defaultBackgroundColor;

  if (element->foregroundColor != Color(0x00000000U))
    foregroundColor = element->foregroundColor;
  else
    foregroundColor = defaultForegroundColor;

  if (element->underline) {
    if (element->underlineColor != Color(0x00000000U))
      underlineColor = element->underlineColor;
    else
      underlineColor = defaultUnderlineColor;
    attrs |= ATTR_UNDERL;
  } else {
    underlineColor = defaultUnderlineColor;
  }

  if (oldBgColor.v != backgroundColor.v || oldFgColor.v != foregroundColor.v || oldUlColor.v != underlineColor.v) {
    markColorsDirty();
    updateGc();
  }

  int pos_x = element->beginX;

  if (element->offsetPixels > 0) {
    drawShift(cur_mon, pos_x, ALIGN_L, element->offsetPixels);
    pos_x += element->offsetPixels;
  }

  font_t* lastFont = nullptr;
  for (int i = 0; i < element->ucsContentLen; i++) {
    uint32_t ucs = element->ucsContent[i];
    font_t *curFont = (font_t*)element->ucsContentFonts[i];

    if (!curFont) {
      if (fontManager.getFontCount() > 0) curFont = fontManager.getFont(0);
      else continue;
    }

    if (curFont != lastFont) {
      if (curFont->ptr) {
        xcb_change_gc(c, gc[GC_DRAW], XCB_GC_FONT, (const uint32_t []) { curFont->ptr });
      }
      lastFont = curFont;
    }

    drawChar(cur_mon, curFont, pos_x, ALIGN_L, ucs);
    pos_x += element->ucsContentCharWidths[i];
  }
}

void Bar::renderAllElements() {
  monitor_t* cur_mon = monhead;
  int RIGHT_MARGIN = fontManager.getSpaceWidth();
  int available_width = cur_mon->width - RIGHT_MARGIN;

  int current_x = 0;
  for (size_t i = 0; i < leftModules.size(); i++) {
    Module* module = leftModules[i];
    for (BarElement* element : module->getElements()) {
      element->beginX = current_x;
      renderElement(element, cur_mon);
      current_x += element->width;
    }
    if (i < leftModules.size() - 1) {
      current_x = renderSeparatorAt(cur_mon, current_x);
    }
  }

  int total_right_width = 0;
  for (Module* module : rightModules) {
    for (BarElement* element : module->getElements()) {
      total_right_width += element->width;
    }
  }

  int right_separator_count = (rightModules.size() > 0) ? (rightModules.size() - 1) : 0;
  int separator_width = separator.totalWidth;

  int total_right_with_separators = total_right_width + (right_separator_count * separator_width);
  current_x = available_width - total_right_with_separators;

  for (size_t i = 0; i < rightModules.size(); i++) {
    Module* module = rightModules[i];
    for (BarElement* element : module->getElements()) {
      element->beginX = current_x;
      renderElement(element, cur_mon);
      current_x += element->width;
    }
    if (i < rightModules.size() - 1) {
      current_x = renderSeparatorAt(cur_mon, current_x);
    }
  }
}

void Bar::parseModulesForSide(const std::vector<Module*>& modules, int startX, bool leftToRight) {
  monitor_t* cur_mon = monhead;
  int current_x = startX;

  for (Module* module : modules) {
    module->window = cur_mon->window;

    for (BarElement* element : module->getElements()) {
      bool colorsChanged = false;
      if (element->backgroundColor != Color(0x00000000U)) {
        backgroundColor = element->backgroundColor;
        colorsChanged = true;
      }
      if (element->foregroundColor != Color(0x00000000U)) {
        foregroundColor = element->foregroundColor;
        colorsChanged = true;
      }
      if (element->underlineColor != Color(0x00000000U)) {
        underlineColor = element->underlineColor;
        colorsChanged = true;
      }
      if (colorsChanged) {
        markColorsDirty();
        updateGc();
      }

      parseElementContent(element);
      element->beginX = current_x;

      if (leftToRight) current_x += element->width;
      else current_x -= element->width;
    }
  }
}

void Bar::parseLeftModules() {
  parseModulesForSide(leftModules, 0, true);
}

void Bar::parseRightModules() {
  monitor_t* cur_mon = monhead;
  int total_width = 0;
  for (Module* module : rightModules) {
    module->window = cur_mon->window;
    for (BarElement* element : module->getElements()) {
      bool colorsChanged = false;
      if (element->backgroundColor != Color(0x00000000U)) {
        backgroundColor = element->backgroundColor;
        colorsChanged = true;
      }
      if (element->foregroundColor != Color(0x00000000U)) {
        foregroundColor = element->foregroundColor;
        colorsChanged = true;
      }
      if (element->underlineColor != Color(0x00000000U)) {
        underlineColor = element->underlineColor;
        colorsChanged = true;
      }
      if (colorsChanged) {
        markColorsDirty();
        updateGc();
      }
      parseElementContent(element);
      total_width += element->width;
    }
  }

  int current_x = cur_mon->width;
  for (Module* module : rightModules) {
    for (BarElement* element : module->getElements()) {
      current_x -= element->width;
      element->beginX = current_x;
    }
  }
}

void Bar::parseModules() {
  monitor_t* cur_mon = monhead;

  for (monitor_t *m = monhead; m != NULL; m = m->next)
    fillRect(m->pixmap, gc[GC_CLEAR], 0, 0, m->width, bh);

  if (!xftDrawInitialized && cur_mon->pixmap) {
    xftDraw = XftDrawCreate(dpy, cur_mon->pixmap, visualPtr, colormap);
    xftDrawInitialized = true;
  } else if (xftDraw) {
    XftDrawChange(xftDraw, cur_mon->pixmap);
  }

  parseLeftModules();
  parseRightModules();
  renderAllElements();
}

void Bar::xconn(void) {
  if ((dpy = XOpenDisplay(0)) == NULL) {
  }
  if ((c = XGetXCBConnection(dpy)) == NULL) {
    exit(EXIT_FAILURE);
  }
  XSetEventQueueOwner(dpy, XCBOwnsEventQueue);
  if (xcb_connection_has_error(c)) {
    exit(EXIT_FAILURE);
  }

  scr = xcb_setup_roots_iterator(xcb_get_setup(c)).data;
  visual = getVisual();
  colormap = xcb_generate_id(c);
  xcb_create_colormap(c, XCB_COLORMAP_ALLOC_NONE, colormap, scr->root, visual);

  fontManager.setDisplay(dpy);
  fontManager.setConnection(c);
}

xcb_visualid_t Bar::getVisual(void) {
  XVisualInfo xv;
  xv.depth = 32;
  int result = 0;
  XVisualInfo* result_ptr = NULL;
  result_ptr = XGetVisualInfo(dpy, VisualDepthMask, &xv, &result);

  if (result > 0) {
    visualPtr = result_ptr->visual;
    return result_ptr->visualid;
  }
  visualPtr = DefaultVisual(dpy, scrNbr);
  return scr->root_visual;
}

void Bar::init(char *wm_name, char *wm_instance) {
  if (!fontManager.getFontCount()) fontManager.loadFont("fixed");
  if (!fontManager.getFontCount()) exit(EXIT_FAILURE);

  int maxh = fontManager.getFontHeight();
  for (int i = 0; i < fontManager.getFontCount(); i++) {
    fontManager.getFont(i)->height = maxh;
  }

  const xcb_query_extension_reply_t *qe_reply;
  monhead = montail = NULL;
  qe_reply = xcb_get_extension_data(c, &xcb_randr_id);

  if (qe_reply && qe_reply->present) {
    getRandrMonitors();
  }

  if (!monhead) {
    if (bw < 0) bw = scr->width_in_pixels - bx;
    if (bh < 0 || bh > scr->height_in_pixels) bh = maxh + bu + 2;
    if (bx + bw > scr->width_in_pixels || by + bh > scr->height_in_pixels) {
      exit(EXIT_FAILURE);
    }
    monhead = monitorNew(0, 0, bw, scr->height_in_pixels);
  }

  if (!monhead) exit(EXIT_FAILURE);

  setEwmhAtoms();

  gc[GC_DRAW] = xcb_generate_id(c);
  xcb_create_gc(c, gc[GC_DRAW], monhead->pixmap, XCB_GC_FOREGROUND, (const uint32_t []){ foregroundColor.v });
  gc[GC_CLEAR] = xcb_generate_id(c);
  xcb_create_gc(c, gc[GC_CLEAR], monhead->pixmap, XCB_GC_FOREGROUND, (const uint32_t []){ backgroundColor.v });
  gc[GC_ATTR] = xcb_generate_id(c);
  xcb_create_gc(c, gc[GC_ATTR], monhead->pixmap, XCB_GC_FOREGROUND, (const uint32_t []){ underlineColor.v });

  for (monitor_t *mon = monhead; mon; mon = mon->next) {
    fillRect(mon->pixmap, gc[GC_CLEAR], 0, 0, mon->width, bh);
    xcb_map_window(c, mon->window);
    xcb_configure_window(c, mon->window, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, (const uint32_t []){ (uint32_t)mon->x, (uint32_t)mon->y });

    if (wm_name)
      xcb_change_property(c, XCB_PROP_MODE_REPLACE, mon->window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, strlen(wm_name), wm_name);

    if (wm_instance) {
      char *wm_class;
      int wm_class_offset, wm_class_len;
      wm_class_offset = strlen(wm_instance) + 1;
      wm_class_len = wm_class_offset + 4;
      wm_class = static_cast<char *>(calloc(1, wm_class_len + 1));
      strcpy(wm_class, wm_instance);
      strcpy(wm_class+wm_class_offset, "Bar");
      xcb_change_property(c, XCB_PROP_MODE_REPLACE, mon->window, XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 8, wm_class_len, wm_class);
      free(wm_class);
    }
  }

  initSystemTray();

  char color[] = "#ffffff";
  uint32_t nfgc = foregroundColor.v & 0x00ffffff;
  snprintf(color, sizeof(color), "#%06X", nfgc);
  XftColorAllocName(dpy, visualPtr, colormap, color, &selFg);
  xcb_flush(c);
}

void Bar::setEwmhAtoms(void) {
  const char *atom_names[] = {
    "_NET_WM_WINDOW_TYPE", "_NET_WM_WINDOW_TYPE_DOCK", "_NET_WM_DESKTOP",
    "_NET_WM_STRUT_PARTIAL", "_NET_WM_STRUT", "_NET_WM_STATE",
    "_NET_WM_STATE_STICKY", "_NET_WM_STATE_ABOVE",
  };
  const int atoms = sizeof(atom_names)/sizeof(char *);
  xcb_intern_atom_cookie_t atom_cookie[atoms];
  xcb_atom_t atom_list[atoms];
  xcb_intern_atom_reply_t *atom_reply;

  for (int i = 0; i < atoms; i++)
    atom_cookie[i] = xcb_intern_atom(c, 0, strlen(atom_names[i]), atom_names[i]);

  for (int i = 0; i < atoms; i++) {
    atom_reply = xcb_intern_atom_reply(c, atom_cookie[i], NULL);
    if (!atom_reply) return;
    atom_list[i] = atom_reply->atom;
    free(atom_reply);
  }

  for (monitor_t *mon = monhead; mon; mon = mon->next) {
    int strut[12] = {0};
    if (topbar) {
      strut[2] = bh;
      strut[8] = mon->x;
      strut[9] = mon->x + mon->width;
    } else {
      strut[3] = bh;
      strut[10] = mon->x;
      strut[11] = mon->x + mon->width;
    }
    xcb_change_property(c, XCB_PROP_MODE_REPLACE, mon->window, atom_list[0], XCB_ATOM_ATOM, 32, 1, &atom_list[1]);
    xcb_change_property(c, XCB_PROP_MODE_APPEND,  mon->window, atom_list[5], XCB_ATOM_ATOM, 32, 2, &atom_list[6]);
    xcb_change_property(c, XCB_PROP_MODE_REPLACE, mon->window, atom_list[2], XCB_ATOM_CARDINAL, 32, 1, (const uint32_t []) { 0u - 1u });
    xcb_change_property(c, XCB_PROP_MODE_REPLACE, mon->window, atom_list[3], XCB_ATOM_CARDINAL, 32, 12, strut);
    xcb_change_property(c, XCB_PROP_MODE_REPLACE, mon->window, atom_list[4], XCB_ATOM_CARDINAL, 32, 4, strut);
    xcb_change_property(c, XCB_PROP_MODE_REPLACE, mon->window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, 3, "bar");
    xcb_change_property(c, XCB_PROP_MODE_REPLACE, mon->window, XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 8, 12, "lemonbar\0Bar");
  }
}

void Bar::initSystemTray(void) {
  trayEnabled = !topbar && montail;
  if (!trayEnabled) return;

  const char* atomNames[] = {
    "_NET_SYSTEM_TRAY_S0",
    "_NET_SYSTEM_TRAY_OPCODE",
    "MANAGER",
    "_XEMBED_INFO"
  };

  xcb_intern_atom_cookie_t cookies[4];
  for (int i = 0; i < 4; i++) {
    cookies[i] = xcb_intern_atom(c, 0, strlen(atomNames[i]), atomNames[i]);
  }

  xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(c, cookies[0], NULL);
  if (!reply) return;
  atomTraySelection = reply->atom;
  free(reply);

  reply = xcb_intern_atom_reply(c, cookies[1], NULL);
  if (!reply) return;
  atomTrayOpcode = reply->atom;
  free(reply);

  reply = xcb_intern_atom_reply(c, cookies[2], NULL);
  if (!reply) return;
  atomManager = reply->atom;
  free(reply);

  reply = xcb_intern_atom_reply(c, cookies[3], NULL);
  if (!reply) return;
  atomXembedInfo = reply->atom;
  free(reply);

  trayWindow = xcb_generate_id(c);


  int trayWindowX = max(0, montail->width - trayWindowWidth);

  const uint32_t values[] = {
    backgroundColor.v,
    XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY
  };
  xcb_create_window(
    c,
    XCB_COPY_FROM_PARENT,
    trayWindow,
    montail->window,
    trayWindowX,

    0,

    trayWindowWidth,
    bh,
    0,
    XCB_WINDOW_CLASS_INPUT_OUTPUT,
    visual,

    XCB_CW_BACK_PIXEL | XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK | XCB_CW_COLORMAP,
    values
  );
  xcb_configure_window(c, trayWindow, XCB_CONFIG_WINDOW_STACK_MODE, (const uint32_t[]){ XCB_STACK_MODE_ABOVE });

    XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK,
    values
  );

  xcb_map_window(c, trayWindow);

  xcb_set_selection_owner(c, trayWindow, atomTraySelection, XCB_CURRENT_TIME);
  sendTrayManagerAnnouncement(trayWindow);
}

void Bar::sendTrayManagerAnnouncement(xcb_window_t owner) {
  if (atomManager == XCB_ATOM_NONE || atomTraySelection == XCB_ATOM_NONE) return;

  xcb_client_message_event_t managerEvent{};
  managerEvent.response_type = XCB_CLIENT_MESSAGE;
  managerEvent.window = scr->root;
  managerEvent.type = atomManager;
  managerEvent.format = 32;
  managerEvent.data.data32[0] = XCB_CURRENT_TIME;
  managerEvent.data.data32[1] = atomTraySelection;
  managerEvent.data.data32[2] = owner;

  xcb_send_event(c, 0, scr->root, XCB_EVENT_MASK_STRUCTURE_NOTIFY, reinterpret_cast<const char*>(&managerEvent));
  xcb_flush(c);
}

void Bar::dockTrayIcon(xcb_window_t iconWindow) {
  if (trayWindow == XCB_WINDOW_NONE || iconWindow == XCB_WINDOW_NONE) return;

  trayIcons.push_back(iconWindow);
  size_t index = trayIcons.size() - 1;
  int step = trayIconSize + trayPadding;
  int x = trayPadding + static_cast<int>(index) * step;
  int y = (bh - trayIconSize) / 2;

  xcb_change_save_set(c, XCB_SET_MODE_INSERT, iconWindow);
  xcb_reparent_window(c, iconWindow, trayWindow, x, y);
  xcb_configure_window(
    c,
    iconWindow,
    XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
    (const uint32_t[]){ static_cast<uint32_t>(x), static_cast<uint32_t>(y), static_cast<uint32_t>(trayIconSize), static_cast<uint32_t>(trayIconSize) }
  );
  xcb_map_window(c, iconWindow);
  xcb_flush(c);
}

void Bar::handleTrayClientMessage(xcb_client_message_event_t *cm) {
  if (!trayEnabled || !cm) return;
  if (cm->type != atomTrayOpcode) return;

  constexpr uint32_t kSystemTrayRequestDock = 0;
  if (cm->data.data32[1] == kSystemTrayRequestDock) {
    dockTrayIcon(cm->data.data32[2]);
  }
}

monitor_t* Bar::monitorNew(int x, int y, int width, int height) {
  monitor_t *ret;
  ret = static_cast<monitor_t *>(calloc(1, sizeof(monitor_t)));
  if (!ret) exit(EXIT_FAILURE);

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
                    (const uint32_t []) { backgroundColor.v, backgroundColor.v, dock, XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS, colormap });

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

void Bar::monitorAdd(monitor_t *mon) {
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

int Bar::rectSortCb(const void *p1, const void *p2) {
  const xcb_rectangle_t *r1 = (xcb_rectangle_t *)p1;
  const xcb_rectangle_t *r2 = (xcb_rectangle_t *)p2;
  if (r1->x < r2->x || r1->y + r1->height <= r2->y) return -1;
  if (r1->x > r2->x || r1->y + r1->height > r2->y) return 1;
  return 0;
}

void Bar::monitorCreateChain(xcb_rectangle_t *rects, const int num) {
  int i;
  int width = 0, height = 0;
  int left = bx;

  qsort(rects, num, sizeof(xcb_rectangle_t), rectSortCb);

  for (i = 0; i < num; i++) {
    int h = rects[i].y + rects[i].height;
    width += rects[i].width;
    if (h >= height) height = h;
  }

  if (bw < 0) bw = width - bx;
  if (bh < 0 || bh > height) bh = fontManager.getFontHeight() + bu + 2;
  if (bx + bw > width || by + bh > height) exit(EXIT_FAILURE);

  width = bw;
  for (i = 0; i < num; i++) {
    if (rects[i].y + rects[i].height < by) continue;
    if (rects[i].width > left) {
      monitor_t *mon = monitorNew(rects[i].x + left, rects[i].y, min(width, rects[i].width - left), rects[i].height);
      if (!mon) break;
      monitorAdd(mon);
      width -= rects[i].width - left;
      if (width <= 0) break;
    }
    left -= rects[i].width;
    if (left < 0) left = 0;
  }
}

void Bar::getRandrMonitors(void) {
  xcb_randr_get_screen_resources_current_reply_t *rres_reply;
  xcb_randr_output_t *outputs;
  int i, j, num, valid = 0;

  rres_reply = xcb_randr_get_screen_resources_current_reply(c, xcb_randr_get_screen_resources_current(c, scr->root), NULL);
  if (!rres_reply) return;

  num = xcb_randr_get_screen_resources_current_outputs_length(rres_reply);
  outputs = xcb_randr_get_screen_resources_current_outputs(rres_reply);

  if (num < 1) {
    free(rres_reply);
    return;
  }

  xcb_rectangle_t rects[num];

  for (i = 0; i < num; i++) {
    xcb_randr_get_output_info_reply_t *oi_reply;
    xcb_randr_get_crtc_info_reply_t *ci_reply;

    oi_reply = xcb_randr_get_output_info_reply(c, xcb_randr_get_output_info(c, outputs[i], XCB_CURRENT_TIME), NULL);
    if (!oi_reply || oi_reply->crtc == XCB_NONE || oi_reply->connection != XCB_RANDR_CONNECTION_CONNECTED) {
      free(oi_reply);
      rects[i].width = 0;
      continue;
    }

    ci_reply = xcb_randr_get_crtc_info_reply(c, xcb_randr_get_crtc_info(c, oi_reply->crtc, XCB_CURRENT_TIME), NULL);
    free(oi_reply);
    if (!ci_reply) {
      free(rres_reply);
      return;
    }

    rects[i] = (xcb_rectangle_t){ ci_reply->x, ci_reply->y, ci_reply->width, ci_reply->height };
    free(ci_reply);
    valid++;
  }

  free(rres_reply);

  for (i = 0; i < num; i++) {
    if (rects[i].width == 0) continue;
    for (j = 0; j < num; j++) {
      if (i != j && rects[j].width) {
        if (rects[j].x >= rects[i].x && rects[j].x + rects[j].width <= rects[i].x + rects[i].width &&
          rects[j].y >= rects[i].y && rects[j].y + rects[j].height <= rects[i].y + rects[i].height) {
          rects[j].width = 0;
          valid--;
        }
      }
    }
  }

  if (valid < 1) return;

  xcb_rectangle_t r[valid];
  for (i = j = 0; i < num && j < valid; i++)
    if (rects[i].width != 0)
      r[j++] = rects[i];

  monitorCreateChain(r, valid);
}

void Bar::getXineramaMonitors(void) {
#ifndef WITH_XINERAMA
  return;
#else
  xcb_xinerama_query_screens_reply_t *xqs_reply;
  xcb_xinerama_screen_info_iterator_t iter;
  int screens;

  xqs_reply = xcb_xinerama_query_screens_reply(c, xcb_xinerama_query_screens_unchecked(c), NULL);
  iter = xcb_xinerama_query_screens_screen_info_iterator(xqs_reply);
  screens = iter.rem;

  xcb_rectangle_t rects[screens];
  for (int i = 0; iter.rem; i++) {
    rects[i].x = iter.data->x_org;
    rects[i].y = iter.data->y_org;
    rects[i].width = iter.data->width;
    rects[i].height = iter.data->height;
    xcb_xinerama_query_screens_next(&iter);
  }

  free(xqs_reply);
  monitorCreateChain(rects, screens);
#endif
}

bool Bar::parseGeometryString(char *str, int *tmp) {
  char *p = str;
  int i = 0, j;

  if (!str || !str[0]) return false;
  if (*p == '=') p++;

  while (*p) {
    if (i >= 4) return false;
    if (*p == 'x') { if (i > 0) break; i++; p++; continue; }
    if (*p == '+') { if (i < 1) i = 2; else i++; p++; continue; }
    if (!isdigit(*p)) return false;
    errno = 0;
    j = strtoul(p, &p, 10);
    if (errno) return false;
    tmp[i] = j;
  }
  return true;
}

char* Bar::strip_path(char *path) {
  char *slash;
  if (path == NULL || *path == '\0') return strdup("lemonbar");
  slash = strrchr(path, '/');
  if (slash != NULL) return strndup(slash + 1, 31);
  return strndup(path, 31);
}

void Bar::sighandle(int signal) {
  if (signal == SIGINT || signal == SIGTERM)
    exit(EXIT_SUCCESS);
}
