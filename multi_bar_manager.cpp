#define _POSIX_C_SOURCE 200809L
#define _BSD_SOURCE

#include "multi_bar_manager.h"
#include "bar.h"
#include "config.h"
#include "modules/workspace.h"
#include "modules/module.h"
#include "bar_manager.h"

#include <X11/Xlib-xcb.h>
#include <sys/select.h>
#include <time.h>
#include <stdio.h>
#include <unistd.h>

MultiBarManager::MultiBarManager(
  const std::vector<Module*> &topLeftModules,
  const std::vector<Module*> &topRightModules,
  const std::vector<Module*> &bottomLeftModules,
  const std::vector<Module*> &bottomRightModules
) :
  topLeftModules(topLeftModules),
  topRightModules(topRightModules),
  bottomLeftModules(bottomLeftModules),
  bottomRightModules(bottomRightModules),
  xcb_fd_top(-1),
  xcb_fd_bottom(-1),
  dpy(nullptr),
  c(nullptr),
  barTop(nullptr),
  barBottom(nullptr)
{
}

MultiBarManager::~MultiBarManager() {
  if (barTop) delete barTop;
  if (barBottom) delete barBottom;
  if (c) xcb_disconnect(c);
  if (dpy) XCloseDisplay(dpy);
}

bool MultiBarManager::initialize() {
  barTop = new Bar(
    "topBar",
    Config::COLOR_BACKGROUND,
    Config::COLOR_FOREGROUND,
    true,
    {std::string(Config::FONT_TEXT), std::string(Config::FONT_ICON)},
    topLeftModules,
    topRightModules
  );

  barBottom = new Bar(
    "bottomBar",
    Config::COLOR_BACKGROUND,
    Config::COLOR_FOREGROUND,
    false,
    {std::string(Config::FONT_TEXT), std::string(Config::FONT_ICON)},
    bottomLeftModules,
    bottomRightModules
  );

  dpy = barTop->dpy;
  c = barTop->c;
  xcb_fd_top = barTop->getXcbFd();
  xcb_fd_bottom = barBottom->getXcbFd();

  for (auto* module : topLeftModules) {
    module->setRenderFunction([this]() { renderBars(); });
  }
  for (auto* module : topRightModules) {
    module->setRenderFunction([this]() { renderBars(); });
  }
  for (auto* module : bottomLeftModules) {
    module->setRenderFunction([this]() { renderBars(); });
  }
  for (auto* module : bottomRightModules) {
    module->setRenderFunction([this]() { renderBars(); });
  }

  topModules.insert(topModules.end(), topLeftModules.begin(), topLeftModules.end());
  topModules.insert(topModules.end(), topRightModules.begin(), topRightModules.end());

  bottomModules.insert(bottomModules.end(), bottomLeftModules.begin(), bottomLeftModules.end());
  bottomModules.insert(bottomModules.end(), bottomRightModules.begin(), bottomRightModules.end());

  allModules.insert(allModules.end(), topModules.begin(), topModules.end());
  allModules.insert(allModules.end(), bottomModules.begin(), bottomModules.end());

  if (!initializeAllModules(topModules)) return false;
  if (!initializeAllModules(bottomModules)) return false;

  return true;
}

void MultiBarManager::run() {
  renderBars();

  while (!gShutdown.load()) {
    struct timeval tv;
    fd_set fds;

    tv.tv_sec = 1;
    tv.tv_usec = 0;

    FD_ZERO(&fds);
    if (xcb_fd_top != -1) FD_SET(xcb_fd_top, &fds);
    if (xcb_fd_bottom != -1) FD_SET(xcb_fd_bottom, &fds);

    int max_fd = -1;
    if (xcb_fd_top != -1) max_fd = xcb_fd_top;
    if (xcb_fd_bottom != -1 && xcb_fd_bottom > max_fd) max_fd = xcb_fd_bottom;

    for (Module* module : allModules) {
      int fd = module->setupSelectFds(fds);
      if (fd > max_fd) max_fd = fd;
    }

    int ret;
    if (max_fd == -1) {
      ret = select(0, NULL, NULL, NULL, &tv);
    } else {
      ret = select(max_fd + 1, &fds, NULL, NULL, &tv);
    }

    if (ret < 0) {
      perror("select error");
      break;
    }

    bool modules_need_update = false;
    for (Module* module : allModules) {
      if (module->handleEvents(fds)) {
        module->update();
        modules_need_update = true;
      }
    }

    if (xcb_fd_top != -1 && FD_ISSET(xcb_fd_top, &fds)) {
      barTop->processXEvents();
    }
    if (xcb_fd_bottom != -1 && FD_ISSET(xcb_fd_bottom, &fds)) {
      barBottom->processXEvents();
    }

    bool should_update = false;
    if (modules_need_update) {
      should_update = true;
    } else if (ret == 0) {
      should_update = true;
    }

    if (should_update) {
      updateModules(topModules);
      updateModules(bottomModules);

      if (hasUpdates()) {
        renderBars();
      }
    }
  }
}

bool MultiBarManager::initializeAllModules(std::vector<Module*>& modules) {
  for (auto* module : modules) {
    if (!module->initialize()) {
      fprintf(stderr, "Failed to initialize %s module\n", module->getName().c_str());
      return false;
    }
  }
  return true;
}

void MultiBarManager::updateModules(std::vector<Module*>& modules) {
  for (Module* module : modules) {
    if (module->checkAndUpdate()) {
      any_updated = true;
    }
  }
}

bool MultiBarManager::hasUpdates() const {
  return any_updated;
}

void MultiBarManager::renderBars() {
  static int renderCount = 0;
  renderCount++;
  fprintf(stderr, "\n[MultiBarManager] Rendering bars, num: %i\n\n", renderCount);
  barTop->feed();
  barBottom->feed();
}
