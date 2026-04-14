// vim:sw=4:ts=4:et:
#ifndef BAR_H
#define BAR_H

#include "bar_types.h"
#include "barElement.h"
#include "modules/module.h"
#include "font_manager.h"

#include <vector>
#include <string>
#include <functional>

#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <xcb/xcb.h>
#include <xcb/xcbext.h>
#include <xcb/randr.h>

#ifdef WITH_XINERAMA
#include <xcb/xinerama.h>
#endif

#include <signal.h>

#ifndef VERSION
#define VERSION "dev"
#endif

#define max(a,b) ((a) > (b) ? (a) : (b))
#define min(a,b) ((a) < (b) ? (a) : (b))

class Bar {

public:
    Bar(
        const char *name,
        const char *_backgroundColor,
        const char *_foregroundColor,
        const bool topBar,
        const std::vector<std::string> &fonts,
        const std::vector<Module*> &leftModules,
        const std::vector<Module*> &rightModules
    );

    Bar(
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
    );

    ~Bar();

    UTF8Result decodeUtf8Char(const char* input);
    int getUtf8CharWidth(uint32_t ucs, font_t* font);

    void setClickHandler(std::function<void(const char *cmd)> cb);
    int getXcbFd(void);
    void processXEvents(void);
    void feed();

    xcb_window_t getBottomWindow() { return montail ? montail->window : (monhead ? monhead->window : 0); }

    FontManager fontManager;

    XftColor selFg;
    XftDraw *xftDraw;
    bool xftDrawInitialized;
    Display *dpy;
    xcb_connection_t *c;

    xcb_screen_t *scr;
    int scrNbr = 0;

    xcb_gcontext_t gc[GC_MAX];
    xcb_visualid_t visual;
    Visual *visualPtr;
    xcb_colormap_t colormap;

    monitor_t *monhead, *montail;

    uint32_t attrs = 0;
    bool dock = false;
    bool topbar = true;
    int bw = -1, bh = -1, bx = 0, by = 0;
    int bu = 1;

    Color foregroundColor, backgroundColor, underlineColor;
    Color defaultForegroundColor, defaultBackgroundColor, defaultUnderlineColor;
    bool colorsDirty = true;
    bool ownsConnection;
    char lastXftColor[8];
    Color lastBgColor, lastFgColor, lastUlColor;

    std::function<void(const char *cmd)> clickCb;
    bool processingExpose = false;

    const std::vector<Module*> leftModules;
    const std::vector<Module*> rightModules;
    std::vector<Module*> modules;

private:
    void fontLoad(const char *pattern);
    void addYOffset(int offset);
    bool fontHasGlyph(font_t *font, const uint32_t c);
    font_t* selectDrawableFont(const uint32_t c);
    int xftCharWidthSlot(uint32_t ch);
    int xftCharWidth(uint32_t ch, font_t* curFont);
    int drawChar(monitor_t* mon, font_t* curFont, int x, int align, uint32_t ch);
    int shift(monitor_t* mon, int x, int align, int chWidth);
    void drawLines(monitor_t* mon, int x, int w);
    void drawShift(monitor_t* mon, int x, int align, int w);
    xcb_void_cookie_t xcb_poly_text_16_simple(xcb_connection_t *c, xcb_drawable_t drawable, xcb_gcontext_t gc, int16_t x, int16_t y, uint32_t len, const uint16_t *str);
    void fillRect(xcb_drawable_t d, xcb_gcontext_t _gc, int x, int y, int width, int height);
    void setAttribute(const char modifier, const char attribute);
    void updateGc(void);
    void markColorsDirty(void);

    struct CachedSeparator {
        uint32_t ucs[2];
        font_t* fonts[2];
        int widths[2];
        int totalWidth;
    };
    CachedSeparator separator;
    void initSeparator();
    int renderSeparatorAt(monitor_t* cur_mon, int current_x);

    void parseElementContent(BarElement* element);
    void renderElement(BarElement* element, monitor_t* cur_mon);
    void renderAllElements();

    void parseModulesForSide(const std::vector<Module*>& modules, int startX, bool leftToRight);
    void parseLeftModules();
    void parseRightModules();
    void parseModules();

    void xconn(void);
    xcb_visualid_t getVisual(void);
    void init(char *wm_name, char *wm_instance);
    void setEwmhAtoms(void);
    void initSystemTray(void);
    void sendTrayManagerAnnouncement(xcb_window_t owner);
    void handleTrayClientMessage(xcb_client_message_event_t *cm);
    void dockTrayIcon(xcb_window_t iconWindow);
    monitor_t* monitorNew(int x, int y, int width, int height);
    void monitorAdd(monitor_t *mon);
    static int rectSortCb(const void *p1, const void *p2);
    void monitorCreateChain(xcb_rectangle_t *rects, const int num);
    void getRandrMonitors(void);
    void getXineramaMonitors(void);
    bool parseGeometryString(char *str, int *tmp);
    char* strip_path(char *path);
    static void sighandle(int signal);

    bool trayEnabled = false;
    int trayIconSize = 22;
    int trayPadding = 5;
    int trayWindowWidth = 200;
    xcb_atom_t atomTraySelection = XCB_ATOM_NONE;
    xcb_atom_t atomTrayOpcode = XCB_ATOM_NONE;
    xcb_atom_t atomManager = XCB_ATOM_NONE;
    xcb_atom_t atomXembedInfo = XCB_ATOM_NONE;
    xcb_window_t trayWindow = XCB_WINDOW_NONE;
    std::vector<xcb_window_t> trayIcons;
};

#endif
