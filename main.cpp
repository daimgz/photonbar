#include "modules/ping.h"
#include "process_manager.h"
#include "multi_bar_manager.h"
#include "config.h"
#define _POSIX_C_SOURCE 200809L
#define _BSD_SOURCE
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/select.h>
#include <time.h>
#include <xcb/xcb.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <memory>
#include <array>
#include <signal.h>
#include <sys/types.h>
#include <pwd.h>
#include <fstream>
#include <libgen.h>
#include <algorithm>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <chrono>

#include "modules/datetime.h"
#include "modules/battery.h"
#include "modules/audio.h"
#include "modules/workspace.h"
#include "modules/resources.h"
#include "modules/stopwatch.h"
#include "modules/timer.h"
#include "modules/weather.h"
#include "modules/space.h"
#include "modules/notifications.h"
#include "modules/tray.h"
#include "bar.h"

int main(int argc, char* argv[]) {
  bool restart_mode = false;
  bool no_lock = false;
  bool kill_only = false;
  bool verbose = true;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--restart") == 0) {
      restart_mode = true;
    } else if (strcmp(argv[i], "--no-lock") == 0) {
      no_lock = true;
    } else if (strcmp(argv[i], "--kill") == 0) {
      kill_only = true;
    } else if (strcmp(argv[i], "--quiet") == 0) {
      verbose = false;
    } else if (strcmp(argv[i], "--help") == 0) {
      printf("Uso: %s [opciones]\n", argv[0]);
      printf("Opciones:\n");
      printf("  --restart    Termina instancia existente y comienza nueva\n");
      printf("  --no-lock    Inicia sin verificar instancias (para debugging)\n");
      printf("  --kill       Solo termina instancias existentes\n");
      printf("  --quiet      Modo silencioso\n");
      printf("  --help       Muestra esta ayuda\n");
      return 0;
    }
  }

  ProcessManager processManager;

  if (kill_only) {
    return processManager.killExistingInstance(verbose) ? 0 : 1;
  }

  if (!no_lock) {
    if (!processManager.handleExistingInstances(restart_mode, verbose)) {
      return 1;
    }
  }

  processManager.setupSignalHandlers();

  static WorkspaceModule workspace_top;
  static AudioModule audio_top;
  static BatteryModule battery_top;
  static DateTimeModule datetime_top;
  static WeatherModule weather_top;
  static NotificationsModule notifications_top;

  static PingModule ping_bottom;
  static TimerModule timer_bottom;
  static StopwatchModule stopwatch_bottom;
  static SpaceModule space_bottom;
  static AudioModule audio_bottom;
  static ResourcesModule resources_bottom;
  static TrayModule tray_bottom;

  fprintf(stderr, "[main] tray_bottom created\n");
  fflush(stderr);

  std::vector<Module*> topLeftModules;
  topLeftModules.push_back(&workspace_top);

  std::vector<Module*> topRightModules;
  topRightModules.push_back(&audio_top);
  topRightModules.push_back(&battery_top);
  topRightModules.push_back(&notifications_top);
  topRightModules.push_back(&weather_top);
  topRightModules.push_back(&datetime_top);

  std::vector<Module*> bottomLeftModules;
  bottomLeftModules.push_back(&timer_bottom);
  bottomLeftModules.push_back(&stopwatch_bottom);

  std::vector<Module*> bottomRightModules;
  bottomRightModules.push_back(&space_bottom);
  bottomRightModules.push_back(&resources_bottom);
  bottomRightModules.push_back(&ping_bottom);
  bottomRightModules.push_back(&tray_bottom);

  MultiBarManager manager(
    topLeftModules,
    topRightModules,
    bottomLeftModules,
    bottomRightModules
  );

  if (!manager.initialize()) {
    fprintf(stderr, "Failed to initialize MultiBarManager\n");
    return 1;
  }

  manager.run();

  processManager.cleanup();
  return 0;
}