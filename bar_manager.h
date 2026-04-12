#ifndef BAR_MANAGER_H
#define BAR_MANAGER_H

#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <condition_variable>

class Module;
class Bar;
class WorkspaceModule;

class BarManager {
public:
  BarManager(
    const char* name,
    const bool isTop,
    const std::vector<Module*> &leftModules,
    const std::vector<Module*> &rightModules
  );

  bool initialize();
  void run();

private:
  const std::string name;
  std::vector<Module*> modules;
  const std::vector<Module*> leftModules;
  const std::vector<Module*> rightModules;
  const bool isTop;

  bool any_updated = false;
  int xcb_fd;
  Bar* bar;
  WorkspaceModule* workspace = nullptr;

  void registerModule(Module* module);
  bool initializeAllModules();
  void updateModules();
  bool hasUpdates() const;
  void renderBar();
  void handleXEvents(fd_set &fds);
};

extern std::atomic<bool> gShutdown;
extern std::mutex gBarManagerMutex;
extern BarManager* gBarManagers[2];
extern std::mutex gInitMutex;
extern std::condition_variable gInitCv;
extern bool gTopReady;

#endif