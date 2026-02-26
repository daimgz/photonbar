#include "modules/ping.h"
#include "process_manager.h"
#define _POSIX_C_SOURCE 200809L
#define _BSD_SOURCE
#define I3IPC_IMPLEMENTATION  // For single-header i3ipc library
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

// --- MÓDULOS PROPIOS ---
#include "modules/datetime.h"
#include "modules/battery.h"
#include "modules/audio.h"
#include "modules/workspace.h"
#include "modules/resources.h"
#include "modules/ping.h"
#include "modules/stopwatch.h"
#include "modules/timer.h"
#include "modules/weather.h"
#include "modules/space.h"
#include "modules/notifications.h"
#include "process_manager.h"
#include "bar.h"

// --- CONFIGURACIÓN VISUAL ---
#define COLOR_BG        "#1A0B2E"
#define COLOR_FG        "#E0AAFF"
#define COLOR_ALERT     "#FF0000"
#define COLOR_SEP       "#E0AAFF"
#define SEP_SYM         " ▏"

// Fuentes
#define FONT_TEXT       "DejaVu Sans Mono:size=16"
#define FONT_ICON       "Symbols Nerd Font:size=16"

class BarManager; // Forward declaration

// Variables globales para manejo de señales y múltiples barras
static std::atomic<bool> gShutdown(false);

// Referencias globales para callbacks de click (compatible con hilos)
static BarManager* gBarManagers[2] = {nullptr, nullptr};
static std::mutex gBarManagerMutex;

// Sincronización para inicialización secuencial
static std::mutex gInitMutex;
static std::condition_variable gInitCv;
static bool gTopReady = false;

class BarManager {
public:
  BarManager(
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
    // Guardar las direcciones de los módulos pasados por parámetro
    for (Module* module : leftModules) {
      modules.push_back(module);
    }

    for (Module* module : rightModules) {
      modules.push_back(module);
    }

    // Encontrar y guardar referencia al módulo workspace
    for (auto* module : modules) {
      module->setRenderFunction([this]() { renderBar(); });
      if (module->getName() == "workspace") {
        workspace = static_cast<WorkspaceModule*>(module);
      }
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

  bool initialize() {
    if (!initializeAllModules()) {
      return false;
    }

    setvbuf(stdout, NULL, _IONBF, 0);

    // Si es la barra superior, notificar que está lista
    if (this->isTop) {
      {
        std::lock_guard<std::mutex> lock(gInitMutex);
        gTopReady = true;
      }
      gInitCv.notify_one();
      fprintf(stderr, "[BarManager] Barra superior inicializada y lista\n");
    }

    return true;
  }

  void run() {
    renderBar(); // Initial render

    while (!gShutdown.load()) {
      struct timeval tv;
      fd_set fds;
      struct timespec ts;

      // Set timeout for ~1 second intervals
      clock_gettime(CLOCK_REALTIME, &ts);
      long usec = 1000000 - (ts.tv_nsec / 1000) + 10000;
      tv.tv_sec = 0; tv.tv_usec = usec;

      FD_ZERO(&fds);
      if (xcb_fd != -1) FD_SET(xcb_fd, &fds); // Escuchar eventos X
      int i3_fd = workspace ? workspace->setupSelectFds(fds) : -1; // Escuchar cambios de escritorio

      // Calcular max_fd correctamente cuando no hay workspace
      int max_fd = -1;
      if (xcb_fd != -1) max_fd = xcb_fd;
      if (i3_fd > max_fd) max_fd = i3_fd;

      int ret;
      // Si no hay file descriptors válidos, usar timeout
      if (max_fd == -1) {
        tv.tv_sec = 1;  // Timeout de 1 segundo si no hay eventos
        tv.tv_usec = 0;
        ret = select(0, NULL, NULL, NULL, &tv);
      } else {
        ret = select(max_fd + 1, &fds, NULL, NULL, &tv);
      }



      if (ret < 0) {
        perror("select error");
        break;
      }

      bool workspace_changed = workspace ? workspace->handleI3Events(fds) : false;

      // Procesar eventos X - estos pueden generar clicks que se manejan por separado
      if (xcb_fd != -1 && FD_ISSET(xcb_fd, &fds)) {
        handleXEvents(fds);
      }

      // Determinar si necesitamos actualizar módulos
      bool should_update = false;
      if (workspace_changed) {
        fprintf(stderr, "[BarManager] Workspace changed, marking for update\n");
        workspace->update();
        should_update = true;
      } else
      if (ret == 0) {
        // Timeout reached - revisar módulos para actualización periódica
        fprintf(stderr, "[BarManager] Timeout reached, checking modules\n");
        should_update = true;
      }
      // Nota: Los clicks se procesan en callbacks asíncronos que llaman a render_bar directamente

      // Actualizar módulos solo si es necesario
      if (should_update) {
        updateModules();

        // Renderizar solo si algún módulo se actualizó
        if (hasUpdates()) {
          renderBar();
        }
      }
    }
  }

private:
  // Arrays constantes
  const std::string name;
  std::vector<Module*> modules;
  const std::vector<Module*> leftModules;
  const std::vector<Module*> rightModules;
  const bool isTop;

  // State
  bool any_updated = false;
  int xcb_fd;
  Bar* bar;
  WorkspaceModule* workspace = nullptr;

  // Eliminados handlers estáticos duplicados - ahora usa lambda con captura this

  // Métodos del scheduler migrados
  void registerModule(Module* module) {
    modules.push_back(module);
  }

  bool initializeAllModules() {
    for (auto* module : modules) {
      if (!module->initialize()) {
        fprintf(stderr, "[BarManager] Failed to initialize %s module\n", module->getName().c_str());
        return false;
      }
    }
    return true;
  }

  void updateModules() {
    any_updated = false;
    for (Module* module : modules) {
      if (module->checkAndUpdate()) {
        any_updated = true;
      }
    }
  }

  bool hasUpdates() const {
    return any_updated;
  }

  void renderBar() {
    // Los módulos ya se actualizaron a través del scheduler
    static int renderCount = 0;

    renderCount++;
    fprintf(stderr, "[BarManager] Rendering bar with content, num: %i", renderCount);
    bar->feed();
  }

  void handleXEvents(fd_set &fds) {
    // 2. EVENTOS X - just process them, don't render
    if (xcb_fd != -1 && FD_ISSET(xcb_fd, &fds)) {
      bar->processXEvents();
    }
  }
};

// --- MAIN LOOP CON HILOS ---
int main(int argc, char* argv[]) {
  bool restart_mode = false;
  bool no_lock = false;
  bool kill_only = false;
  bool verbose = true;

  // Debug logging: Imprimir información de entorno
  FILE* debug_log = fopen("/tmp/myBar_debug.log", "a");
  if (debug_log) {
    fprintf(debug_log, "=== myBar Inicio ===\n");
    fprintf(debug_log, "Timestamp: %ld\n", time(nullptr));
    fprintf(debug_log, "User: %s\n", getenv("USER") ? getenv("USER") : "unknown");
    fprintf(debug_log, "Home: %s\n", getenv("HOME") ? getenv("HOME") : "unknown");
    fprintf(debug_log, "Display: %s\n", getenv("DISPLAY") ? getenv("DISPLAY") : "unknown");
    fprintf(debug_log, "Path: %s\n", getenv("PATH") ? getenv("PATH") : "unknown");
    fprintf(debug_log, "PWD: %s\n", getenv("PWD") ? getenv("PWD") : "unknown");
    fprintf(debug_log, "Args:");
    for (int i = 0; i < argc; i++) {
      fprintf(debug_log, " [%d]=%s", i, argv[i]);
    }
    fprintf(debug_log, "\n");
    fflush(debug_log);
  }

  // Parsear argumentos
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

  // Crear ProcessManager
  ProcessManager processManager;

  // Manejo especial: solo kill
  if (kill_only) {
    return processManager.killExistingInstance(verbose) ? 0 : 1;
  }

  // Manejo de instancias existentes
  if (!no_lock) {
    if (!processManager.handleExistingInstances(restart_mode, verbose)) {
      return 1;
    }
  }

  // Setup de señales para cleanup elegante
  processManager.setupSignalHandlers();

  // Inicializar BarManager
  if (debug_log) {
    fprintf(debug_log, "Iniciando inicialización secuencial de barras...\n");
    fflush(debug_log);
  }

  // Crear módulos fuera de los hilos para evitar problemas de lifetime
  static WorkspaceModule workspace_top;
  static AudioModule audio_top;
  static BatteryModule battery_top;
  static DateTimeModule datetime_top;
  static WeatherModule weather_top;
  static NotificationsModule notifications_top;

  // Módulos para barra inferior
  static PingModule ping_bottom;
  static TimerModule timer_bottom;
  static StopwatchModule stopwatch_bottom;
  static SpaceModule space_bottom;
  static AudioModule audio_bottom;
  static ResourcesModule resources_bottom;

  // Thread para inicializar y ejecutar barra superior
  std::thread top_thread([debug_log]() {
    std::vector<Module*> leftModules;
    leftModules.push_back(&workspace_top);

    std::vector<Module*> rightModules;
    rightModules.push_back(&audio_top);
    rightModules.push_back(&battery_top);
    rightModules.push_back(&notifications_top);
    rightModules.push_back(&weather_top);
    rightModules.push_back(&datetime_top);

    BarManager barTop(
      "topBar",
      true,
      leftModules,
      rightModules
    );

    // Registrar referencia global para callbacks
    {
      std::lock_guard<std::mutex> lock(gBarManagerMutex);
      gBarManagers[0] = &barTop;
    }

    if (debug_log) {
      FILE* debug = fopen("/tmp/myBar_debug.log", "a");
      fprintf(debug, "Iniciando inicialización barra superior...\n");
      fclose(debug);
    }

    if (!barTop.initialize()) {
      fprintf(stderr, "Failed to initialize top BarManager\n");
      return;
    }

    if (debug_log) {
      FILE* debug = fopen("/tmp/myBar_debug.log", "a");
      fprintf(debug, "Barra superior lista, iniciando run()...\n");
      fclose(debug);
    }

    barTop.run();
  });

  // Esperar a que la barra superior esté completamente inicializada
  {
    std::unique_lock<std::mutex> lock(gInitMutex);
    gInitCv.wait(lock, []{ return gTopReady; });
  }

  if (debug_log) {
    fprintf(debug_log, "Barra superior lista, iniciando barra inferior...\n");
    fflush(debug_log);
  }

  // Thread para inicializar y ejecutar barra inferior
  std::thread bottom_thread([debug_log]() {
    std::vector<Module*> leftModules;
    leftModules .push_back(&timer_bottom);
    leftModules .push_back(&stopwatch_bottom);

    std::vector<Module*> rightModules;
    rightModules.push_back(&space_bottom);
    rightModules.push_back(&resources_bottom);
    rightModules.push_back(&ping_bottom);

    BarManager barBottom(
      "bottomBar",
      false,
      leftModules,
      rightModules
    );

    // Registrar referencia global para callbacks
    {
      std::lock_guard<std::mutex> lock(gBarManagerMutex);
      gBarManagers[1] = &barBottom;
    }

    if (debug_log) {
      FILE* debug = fopen("/tmp/myBar_debug.log", "a");
      fprintf(debug, "Iniciando inicialización barra inferior...\n");
      fclose(debug);
    }

    if (!barBottom.initialize()) {
      fprintf(stderr, "Failed to initialize bottom BarManager\n");
      return;
    }

    if (debug_log) {
      FILE* debug = fopen("/tmp/myBar_debug.log", "a");
      fprintf(debug, "Barra inferior lista, iniciando run()...\n");
      fclose(debug);
    }

    barBottom.run();
  });

  if (debug_log) {
    fprintf(debug_log, "Ambas barras en ejecución en hilos separados\n");
    fflush(debug_log);
  }

  if (debug_log) {
    fprintf(debug_log, "Ambas barras iniciadas, esperando...\n");
    fflush(debug_log);
  }

  // Esperar a que los hilos terminen
  top_thread.join();
  bottom_thread.join();

  // Cleanup al salir (normalmente no se llega aquí por signal handlers)
  if (debug_log) {
    fprintf(debug_log, "Cleanup final - myBar terminando\n");
    fclose(debug_log);
  }
  processManager.cleanup();
  return 0;
}
