#define _POSIX_C_SOURCE 200809L
#define _BSD_SOURCE

#include "bar_manager.h"
#include "bar.h"
#include "config.h"
#include "modules/module.h"

#include <sys/select.h>
#include <time.h>
#include <stdio.h>
#include <unistd.h>

std::atomic<bool> gShutdown(false);
std::mutex gBarManagerMutex;
BarManager* gBarManagers[2] = {nullptr, nullptr};
std::mutex gInitMutex;
std::condition_variable gInitCv;
bool gTopReady = false;

BarManager::BarManager(
  const char* name,
  const bool isTop,
  const std::vector<Module*> &leftModules,
  const std::vector<Module*> &rightModules
) :
  name(name),
  leftModules(leftModules),
  rightModules(rightModules),
  isTop(isTop),
  xcb_fd(-1)
{
  for (Module* module : leftModules) {
    modules.push_back(module);
  }

  for (Module* module : rightModules) {
    modules.push_back(module);
  }

  for (auto* module : modules) {
    module->setRenderFunction([this]() { renderBar(); });
  }

  bar = new Bar(
    name,
    COLOR_BG,
    COLOR_FG,
    this->isTop,
    {std::string(FONT_TEXT), std::string(FONT_ICON)},
    leftModules,
    rightModules
  );
  xcb_fd = bar->getXcbFd();
}

bool BarManager::initialize() {
  if (!initializeAllModules()) {
    return false;
  }

  setvbuf(stdout, NULL, _IONBF, 0);

  if (this->isTop) {
    {
      std::lock_guard<std::mutex> lock(gInitMutex);
      gTopReady = true;
    }
    gInitCv.notify_one();
    #if DEBUG
    fprintf(stderr, "[BarManager] Barra superior inicializada y lista\n");
    #endif
  }

  return true;
}

void BarManager::run() {
  renderBar();

  while (!gShutdown.load()) {
    struct timeval tv;
    fd_set fds;

    tv.tv_sec = 1;
    tv.tv_usec = 0;

    FD_ZERO(&fds);
    if (xcb_fd != -1) FD_SET(xcb_fd, &fds);

    int max_fd = -1;
    if (xcb_fd != -1) max_fd = xcb_fd;

    for (Module* m : modules) {
      int fd = m->setupSelectFds(fds);
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

    bool module_event = false;
    for (Module* m : modules) {
      if (m->handleEvents(fds)) {
        m->update();
        module_event = true;
      }
    }

    if (xcb_fd != -1 && FD_ISSET(xcb_fd, &fds)) {
      handleXEvents(fds);
    }

    bool should_update = false;
    if (module_event) {
      #if DEBUG
      fprintf(stderr, "[BarManager] Module event detected\n");
      #endif
      should_update = true;
    } else if (ret == 0) {
      #if DEBUG
      fprintf(stderr, "[BarManager] Timeout reached\n");
      #endif
      should_update = true;
    }

    if (should_update) {
      updateModules();

      if (hasUpdates()) {
        renderBar();
      }
    }
  }
}

void BarManager::registerModule(Module* module) {
  modules.push_back(module);
}

bool BarManager::initializeAllModules() {
  for (auto* module : modules) {
    if (!module->initialize()) {
      fprintf(stderr, "[BarManager] Failed to initialize %s module\n", module->getName().c_str());
      return false;
    }
  }
  return true;
}

void BarManager::updateModules() {
  any_updated = false;
  for (Module* module : modules) {
    if (module->checkAndUpdate()) {
      any_updated = true;
    }
  }
}

bool BarManager::hasUpdates() const {
  return any_updated;
}

void BarManager::renderBar() {
  #if DEBUG
  static int renderCount = 0;
  renderCount++;
  fprintf(stderr, "[BarManager] Rendering bar, num: %i\n", renderCount);
  #endif
  bar->feed();
}

void BarManager::handleXEvents(fd_set &fds) {
  if (xcb_fd != -1 && FD_ISSET(xcb_fd, &fds)) {
    bar->processXEvents();
  }
}