#ifndef WORKSPACE_H
#define WORKSPACE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

#define I3IPC_IMPLEMENTATION
#include "i3ipc.h"
#include "module.h"

// Static cache variables (outside class to avoid linker issues)
static char current_workspace[128];
static bool cache_initialized = false;

// Variables para watchdog del loop (completamente en workspace.h)
int i3_events_this_second = 0;
int last_second = 0;

class WorkspaceModule : public Module {
  public:
    WorkspaceModule():
      Module("workspace")
    {}

    void update() {
      // Only initialize cache once
      if (!cache_initialized) {
        refresh_cache();
      }
      
      // Generate buffer from cached workspace names
      generate_buffer();
    }

    void event(const char* eventValue) {
      // Handle workspace switching events
      if (strncmp(eventValue, "ws_", 3) == 0) {
        const char *ws_name = eventValue + 3;
        
        // 1. Instantáneo: actualizar cache y buffer (como datetime)
        update_cache(ws_name);
        generate_buffer();
        
        // 2. Ejecutar comando real de cambio de workspace
        execute_workspace_switch(ws_name);
      }
    }

    // Force cache refresh from i3 state (for external i3 command sync)
    void force_cache_refresh() {
      refresh_cache();
      generate_buffer();
    }

    // Initialize workspace module (incluye i3 subscription)
    bool initialize() {
      i3_fd = subscribe_i3();
      return i3_fd != -1;
    }

    // Setup select file descriptors for main loop
    int setup_select_fds(fd_set &fds) {
        if (i3_fd != -1) {
            FD_SET(i3_fd, &fds);
            return i3_fd;
        }
        return -1;
    }

    // Handle i3 events from main loop (mover desde main.cpp)
    bool handle_i3_events(fd_set &fds);

    // Get i3 file descriptor for select loop
    int get_i3_fd() const { return i3_fd; }

  private:
    int i3_fd; // File descriptor for i3 events

    void refresh_cache() {
      // Get current workspace using i3ipc
      I3ipc_reply_workspaces* reply = i3ipc_get_workspaces();
      if (!reply) return;
      
      for (int i = 0; i < reply->workspaces_size; ++i) {
        if (reply->workspaces[i].focused) {
          strncpy(current_workspace, reply->workspaces[i].name, sizeof(current_workspace) - 1);
          current_workspace[sizeof(current_workspace) - 1] = '\0';
          break;
        }
      }
      
      cache_initialized = true;
      free(reply);
    }
    
    void generate_buffer() {
      // Generate workspace display buffer
      char result[1024] = "";
      
      I3ipc_reply_workspaces* reply = i3ipc_get_workspaces();
      if (reply) {
        for (int i = 0; i < reply->workspaces_size; ++i) {
          bool is_current = (strcmp(reply->workspaces[i].name, current_workspace) == 0);
          
          if (is_current) {
            sprintf(result + strlen(result), 
                "%%{A1:ws_%s:}%%{F#E0AAFF}%%{U#E0AAFF}%%{+u} %s %%{-u}%%{U-}%%{F-}%%{A}", 
                reply->workspaces[i].name, reply->workspaces[i].name);
          } else {
            sprintf(result + strlen(result), 
                "%%{A1:ws_%s:}%%{F#666666} %s %%{F-}%%{A}", 
                reply->workspaces[i].name, reply->workspaces[i].name);
          }
        }
        free(reply);
      }
      
      buffer = result;
    }
    
    void update_cache(const char* ws_name) {
      // Instantly update current workspace in cache (no IPC needed)
      strncpy(current_workspace, ws_name, sizeof(current_workspace) - 1);
      current_workspace[sizeof(current_workspace) - 1] = '\0';
    }
    
    void execute_workspace_switch(const char* ws_name) {
      // Execute workspace switch using non-blocking approach
      char cmd[256];
      snprintf(cmd, sizeof(cmd), "i3-msg workspace \"%.50s\" > /dev/null", ws_name);
      
      // Create child process - parent continues immediately
      if (fork() == 0) {
        // Child process - runs i3-msg command
        execlp("i3-msg", "i3-msg", "workspace", ws_name, NULL);
        exit(1); // Only reached if execlp fails
      }
      // Parent process continues without waiting
    }

    int subscribe_i3() {
      int pipefd[2];
      if (pipe(pipefd) == -1) {
        perror("pipe");
        return -1;
      }

      pid_t pid = fork();
      if (pid == -1) {
        perror("fork");
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
      }

      if (pid == 0) {
        // Proceso hijo: ejecutar i3-msg
        close(pipefd[0]); // Cerrar extremo de lectura
        dup2(pipefd[1], STDOUT_FILENO); // Redirigir stdout al pipe
        close(pipefd[1]);

        execlp("i3-msg", "i3-msg", "-t", "subscribe", "[\"workspace\"]", NULL);
        exit(1); // Solo se llega aquí si execlp falla
      } else {
        // Proceso padre: usar el pipe para leer
        close(pipefd[1]); // Cerrar extremo de escritura

        // Hacer que el pipe sea no bloqueante
        int flags = fcntl(pipefd[0], F_GETFL, 0);
        fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK);

        return pipefd[0];
      }
    }

    bool parse_workspace_change_event(const char* json_data) {
      // Parse JSON data to detect workspace changes
      if (strstr(json_data, "\"change\":\"focus\"") ||
          strstr(json_data, "\"change\":\"init\"") ||
          strstr(json_data, "\"change\":\"empty\"") ||
          strstr(json_data, "\"change\":\"urgent\"")) {
        return true;
      }
      return false;
    }

    bool should_ignore_workspace_event() {
      // Watchdog completo: limitar eventos i3 por segundo
      time_t now = time(NULL);
      int current_second = now;

      // Resetear contador si cambió el segundo
      if (current_second != last_second) {
        i3_events_this_second = 0;
        last_second = current_second;
      }

      // Incrementar contador de eventos
      i3_events_this_second++;

      // Si Hay demasiados eventos en un segundo, ignorar el resto
      if (i3_events_this_second > 20) {
        fprintf(stderr, "[WorkspaceModule] Too many i3 events (%d this second), ignoring excess\n", i3_events_this_second);
        return true;
      } else {
        fprintf(stderr, "[WorkspaceModule] i3 workspace event detected (%d this second)\n", i3_events_this_second);
        return false;
      }
    }
};

inline bool WorkspaceModule::handle_i3_events(fd_set &fds) {
  bool workspace_changed = false;

  // 1. EVENTO DE I3 (Cambio de escritorio) - render immediately
  if (i3_fd != -1 && FD_ISSET(i3_fd, &fds)) {
    char buffer[4096];
    fprintf(stderr, "[WorkspaceModule] i3_fd is ready for reading\n");

    // Leemos y procesamos el JSON de i3
    ssize_t bytes_read;
    while ((bytes_read = read(i3_fd, buffer, sizeof(buffer) - 1)) > 0) {
      buffer[bytes_read] = '\0';

      // Parsear evento usando método local
      if (parse_workspace_change_event(buffer)) {
        workspace_changed = true;
        fprintf(stderr, "[WorkspaceModule] Detected workspace change event\n");
      } else {
        fprintf(stderr, "[WorkspaceModule] Read %zd bytes from i3_fd (ignored)\n", bytes_read);
      }
    }

    if (bytes_read < 0) {
      fprintf(stderr, "[WorkspaceModule] Error reading from i3_fd: %s\n", strerror(errno));
    } else if (bytes_read == 0) {
      fprintf(stderr, "[WorkspaceModule] EOF on i3_fd - pipe closed\n");
      close(i3_fd);
      i3_fd = subscribe_i3();
    }

    // Watchdog: aplicar filtro de eventos
    if (should_ignore_workspace_event()) {
      workspace_changed = false;
    }

    // Sync workspace cache when external i3 events are detected
    if (workspace_changed) {
      force_cache_refresh();
      fprintf(stderr, "[WorkspaceModule] Refreshing workspace cache after external change\n");
    }
  }

  return workspace_changed;
}

#endif // WORKSPACE_H