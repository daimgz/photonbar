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
#include <chrono>

// --- MÓDULOS PROPIOS ---
#include "modules/datetime.h"
//#include "modules/battery.h"
//#include "modules/audio.h"
//#include "../modules/workspace.h"
//#include "modules/resources.h"
//#include "modules/ping.h"
//#include "modules/stopwatch.h"
//#include "modules/timer.h"
//#include "modules/weather.h"
//#include "modules/space.h"
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

// Variable global para manejo de señales
static bool g_shutdown = false;

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
    if (leftModules.empty())
      return;

    // Guardar las direcciones de los módulos pasados por parámetro
    for (Module* module : leftModules) {
        modules.push_back(module);
    }

    for (Module* module : rightModules) {
        modules.push_back(module);
    }

    // Encontrar y guardar referencia al módulo workspace
    for (auto* module : modules) {
        module->setRenderFunction([this]() { render_bar(); });
        //if (module->getName() == "workspace") {
            //workspace = static_cast<WorkspaceModule*>(module);
            //break;
        //}
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

      fprintf(stderr, "[BarManager] Barra inicializada y lista\n");
      return true;
  }

  void run() {
      render_bar(); // Initial render

      while (!g_shutdown) {
          struct timeval tv;
          fd_set fds;
          struct timespec ts;

          // Set timeout for ~1 second intervals
          clock_gettime(CLOCK_REALTIME, &ts);
          long usec = 1000000 - (ts.tv_nsec / 1000) + 10000;
          tv.tv_sec = 0; tv.tv_usec = usec;

           FD_ZERO(&fds);
           if (xcb_fd != -1) FD_SET(xcb_fd, &fds); // Escuchar eventos X
           //int i3_fd = workspace ? workspace->setup_select_fds(fds) : -1; // Escuchar cambios de escritorio

           // Calcular max_fd correctamente cuando no hay workspace
           int max_fd = -1;
           if (xcb_fd != -1) max_fd = xcb_fd;
           //if (i3_fd > max_fd) max_fd = i3_fd;

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

           //bool workspace_changed = workspace ? workspace->handle_i3_events(fds) : false;

           // Procesar eventos X - estos pueden generar clicks que se manejan por separado
           if (xcb_fd != -1 && FD_ISSET(xcb_fd, &fds)) {
               handle_x_events(fds);
           }

           // Determinar si necesitamos actualizar módulos
           bool should_update = false;
           //if (workspace_changed) {
              ////TODO: ver si funciona con el nuevo sistema de eventos
               //fprintf(stderr, "[BarManager] Workspace changed, marking for update\n");
               ////markForUpdate("workspace");
               //workspace->update();
               ////hasUpdates()
               //should_update = true;
           //} else
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
                   render_bar();
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

  // Variables del scheduler migradas
  bool any_updated = false;

  int xcb_fd;
  Bar* bar;
  //WorkspaceModule* workspace;

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

  //void markForUpdate(const std::string& moduleName) {
      //auto it = std::find_if(modules.begin(), modules.end(),
          //[&moduleName](Module* m) { return m->getName() == moduleName; });
      //if (it != modules.end()) {
          //(*it)->markForUpdate();
      //}
  //}

  bool hasUpdates() const {
      return any_updated;
  }

  //void setAllAutoUpdate(bool enabled) {
      //for (auto* module : modules) {
          //module->setAutoUpdate(enabled);
      //}
  //}

  void render_bar() {
      // Los módulos ya se actualizaron a través del scheduler
      //char buf[8000];
      static int renderCount = 0;

      //// Construir buffer
      //std::string output = "%{l}";

      //// Módulos izquierdos con separadores
      //bool first_left = true;
      //for (Module* module : leftModules) {
          //if (!first_left) {
              //output += "%{F" + std::string(COLOR_SEP) + "}" + SEP_SYM + "%{F-}";
          //}
          //first_left = false;

          //output += "%{F" + std::string(COLOR_FG) + "}";
          //output += module->getBuffer();
      //}

      //output += "%{r}";

      //// Módulos derechos con separadores
      //bool first = true;
      //for (Module* module : rightModules) {
          //if (!first) {
              //output += "%{F" + std::string(COLOR_SEP) + "}" + SEP_SYM + "%{F-}";
          //}
          //first = false;

          //output += "%{F" + std::string(COLOR_FG) + "}";
          //output += module->getBuffer();
      //}

      //output += " %{F" + std::string(COLOR_SEP) + "}\n";

      //int n = snprintf(buf, sizeof(buf), "%s", output.c_str());
      //if (n > 0) {
          renderCount++;
          fprintf(stderr, "[BarManager] Rendering bar with content, num: %i", renderCount);
          //std::cout << std::endl << std::endl << "main.cpp estoy" << elements[0]->content << " len " << elements[0]->contentLen << std::endl << std::endl << std::endl;
          bar->feed();
      //}
  }

  void handle_x_events(fd_set &fds) {
      // 2. EVENTOS X - just process them, don't render
      if (xcb_fd != -1 && FD_ISSET(xcb_fd, &fds)) {
           bar->processXEvents();
      }
  }
};

// --- MAIN LOOP SIMPLIFICADO ---
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
        fprintf(debug_log, "Iniciando inicialización de barra única...\n");
        fflush(debug_log);
    }

    // Crear módulos para la barra superior
    static DateTimeModule datetime_top;
    static PingModule ping_top;

    std::vector<Module*> left_modules;
    left_modules.push_back(&datetime_top);

    std::vector<Module*> right_modules;
    right_modules.push_back(&ping_top);

    if (debug_log) {
        FILE* debug = fopen("/tmp/myBar_debug.log", "a");
        fprintf(debug, "Creando BarManager para barra superior...\n");
        fclose(debug);
    }

    BarManager barTop(
      "topBar",
      true,
      left_modules,
      right_modules
    );

    if (debug_log) {
        FILE* debug = fopen("/tmp/myBar_debug.log", "a");
        fprintf(debug, "Iniciando inicialización barra superior...\n");
        fclose(debug);
    }

    if (!barTop.initialize()) {
        fprintf(stderr, "Failed to initialize top BarManager\n");
        return 1;
    }

    if (debug_log) {
        FILE* debug = fopen("/tmp/myBar_debug.log", "a");
        fprintf(debug, "Barra superior lista, iniciando run()...\n");
        fclose(debug);
    }

    if (debug_log) {
        fprintf(debug_log, "Barra en ejecución modo single-thread\n");
        fflush(debug_log);
    }

    // Ejecutar la barra directamente (sin hilos)
    barTop.run();

    // Cleanup al salir (normalmente no se llega aquí por signal handlers)
    if (debug_log) {
        fprintf(debug_log, "Cleanup final - myBar terminando\n");
        fclose(debug_log);
    }
    processManager.cleanup();
    return 0;
}
