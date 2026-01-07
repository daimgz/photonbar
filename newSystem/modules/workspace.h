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
#include <vector>
#include <string>
#include <algorithm>

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
      Module("workspace", true, 1)
    {
      initialize_workspace_elements();
    }

    ~WorkspaceModule() {
      clear_workspace_elements();
    }

    void update() {
      // Only initialize cache once
      if (!cache_initialized) {
        refresh_cache();
      }

      // Update workspace elements from current state
      update_workspace_elements();
    }

    // Force cache refresh from i3 state (for external i3 command sync)
    void force_cache_refresh() {
      refresh_cache();
      update_workspace_elements();
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
    std::vector<BarElement*> workspace_elements; // Dynamic workspace elements

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

    void initialize_workspace_elements() {
      clear_workspace_elements();
      
      I3ipc_reply_workspaces* reply = i3ipc_get_workspaces();
      if (!reply) return;

      for (int i = 0; i < reply->workspaces_size; ++i) {
        BarElement* element = new BarElement();
        element->moduleName = name;
        
        // Copy workspace name with spacing
        element->contentLen = snprintf(
            element->content, 
            CONTENT_MAX_LEN - 1, 
            " %s ", 
            reply->workspaces[i].name
        );
        element->content[element->contentLen] = '\0';
        element->dirtyContent = true;
        
        // Set click event with captured workspace name
        std::string ws_name = reply->workspaces[i].name;
        element->setEvent(BarElement::CLICK_LEFT, [this, ws_name]() {
          this->handle_workspace_click(ws_name.c_str());
        });
        
        workspace_elements.push_back(element);
        elements.push_back(element);
      }
      
      free(reply);
    }

    void clear_workspace_elements() {
      for (BarElement* element : workspace_elements) {
        elements.erase(std::remove(elements.begin(), elements.end(), element), elements.end());
        delete element;
      }
      workspace_elements.clear();
    }

    void update_workspace_elements() {
      I3ipc_reply_workspaces* reply = i3ipc_get_workspaces();
      if (!reply) return;

      // Update existing elements and add new ones if needed
      for (int i = 0; i < reply->workspaces_size; ++i) {
        bool is_current = (strcmp(reply->workspaces[i].name, current_workspace) == 0);
        
        // Find or create element for this workspace
        BarElement* element = nullptr;
        for (BarElement* elem : workspace_elements) {
          // Compare trimmed content (remove leading/trailing spaces)
          std::string elem_content(elem->content);
          size_t start = elem_content.find_first_not_of(" ");
          size_t end = elem_content.find_last_not_of(" ");
          if (start != std::string::npos && end != std::string::npos) {
            std::string trimmed = elem_content.substr(start, end - start + 1);
            if (trimmed == reply->workspaces[i].name) {
              element = elem;
              break;
            }
          }
        }
        
        if (!element && i < workspace_elements.size()) {
          // Reuse existing element
          element = workspace_elements[i];
        }
        
        if (!element) {
          // Create new element
          element = new BarElement();
          element->moduleName = name;
          
          std::string ws_name = reply->workspaces[i].name;
          element->setEvent(BarElement::CLICK_LEFT, [this, ws_name]() {
            this->handle_workspace_click(ws_name.c_str());
          });
          
          workspace_elements.push_back(element);
          elements.push_back(element);
        }
        
        // Update element content and styling with spacing
        element->contentLen = snprintf(
            element->content, 
            CONTENT_MAX_LEN - 1, 
            " %s ", 
            reply->workspaces[i].name
        );
        element->content[element->contentLen] = '\0';
        element->dirtyContent = true;
        
        // Apply colors and formatting
        if (is_current) {
          // Active workspace: purple with underline
          element->foregroundColor = Color::parse_color("#E0AAFF", NULL, Color(0xFF, 0xAA, 0xFF, 255));
          element->underlineColor = Color::parse_color("#E0AAFF", NULL, Color(0xFF, 0xAA, 0xFF, 255));
          element->underline = true;
        } else {
          // Inactive workspace: gray
          element->foregroundColor = Color::parse_color("#666666", NULL, Color(0x66, 0x66, 0x66, 255));
          element->underline = false;
        }
      }
      
      // Remove elements for workspaces that no longer exist
      auto it = workspace_elements.begin();
      while (it != workspace_elements.end()) {
        bool found = false;
        for (int i = 0; i < reply->workspaces_size; ++i) {
          // Compare trimmed content (remove leading/trailing spaces)
          std::string elem_content((*it)->content);
          size_t start = elem_content.find_first_not_of(" ");
          size_t end = elem_content.find_last_not_of(" ");
          if (start != std::string::npos && end != std::string::npos) {
            std::string trimmed = elem_content.substr(start, end - start + 1);
            if (trimmed == reply->workspaces[i].name) {
              found = true;
              break;
            }
          }
        }
        if (!found) {
          BarElement* element = *it;
          elements.erase(std::remove(elements.begin(), elements.end(), element), elements.end());
          delete element;
          it = workspace_elements.erase(it);
        } else {
          ++it;
        }
      }
      
      free(reply);
    }

    void handle_workspace_click(const char* ws_name) {
      // 1. Instantáneo: actualizar cache y elementos
      update_cache(ws_name);
      update_workspace_elements();

      // 2. Ejecutar comando real de cambio de workspace
      execute_workspace_switch(ws_name);
      
      // Trigger render
      if (renderFunction) {
        renderFunction();
      }
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