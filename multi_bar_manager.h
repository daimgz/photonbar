#ifndef MULTI_BAR_MANAGER_H
#define MULTI_BAR_MANAGER_H

#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <xcb/xcb.h>
#include <X11/Xlib.h>

class Module;
class Bar;
class WorkspaceModule;

class MultiBarManager {
public:
  MultiBarManager(
    const std::vector<Module*> &topLeftModules,
    const std::vector<Module*> &topRightModules,
    const std::vector<Module*> &bottomLeftModules,
    const std::vector<Module*> &bottomRightModules
  );

  ~MultiBarManager();

  bool initialize();
  void run();

private:
  std::vector<Module*> topModules;
  std::vector<Module*> bottomModules;
  std::vector<Module*> allModules;

  const std::vector<Module*> topLeftModules;
  const std::vector<Module*> topRightModules;
  const std::vector<Module*> bottomLeftModules;
  const std::vector<Module*> bottomRightModules;

  bool any_updated = false;
  int xcb_fd_top;
  int xcb_fd_bottom;
  Display* dpy;
  xcb_connection_t* c;

  Bar* barTop;
  Bar* barBottom;

  void registerModule(Module* module);
  bool initializeAllModules(std::vector<Module*>& modules);
  void updateModules(std::vector<Module*>& modules);
  bool hasUpdates() const;
  void renderBars();
  void handleXEvents(fd_set &fds);
};

#endif