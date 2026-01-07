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

#define I3IPC_IMPLEMENTATION
#include "i3ipc.h"
#include "module.h"

class WorkspaceModule : public Module {
  public:
    WorkspaceModule() : Module("workspace", true, 1) {
      initialize_elements();
    }

    ~WorkspaceModule() {
      clear_elements();
    }

    void update() {
      update_elements();
    }

    bool initialize() {
      i3_fd = subscribe_i3();
      return i3_fd != -1;
    }

    int setup_select_fds(fd_set &fds) {
      if (i3_fd != -1) {
        FD_SET(i3_fd, &fds);
        return i3_fd;
      }
      return -1;
    }

    bool handle_i3_events(fd_set &fds);

  private:
    int i3_fd;
    std::vector<BarElement*> workspace_elements;

    void initialize_elements() {
      clear_elements();
      
      I3ipc_reply_workspaces* reply = i3ipc_get_workspaces();
      if (!reply) return;

      for (int i = 0; i < reply->workspaces_size; ++i) {
        BarElement* element = new BarElement();
        element->moduleName = name;
        
        // Copy workspace name with spaces
        element->contentLen = snprintf(element->content, CONTENT_MAX_LEN - 1, 
                                   " %s ", reply->workspaces[i].name);
        element->content[element->contentLen] = '\0';
        element->dirtyContent = true;
        
        // Set click event
        std::string ws_name = reply->workspaces[i].name;
        element->setEvent(BarElement::CLICK_LEFT, [this, ws_name]() {
          switch_to_workspace(ws_name.c_str());
        });
        
        // Set styling
        if (reply->workspaces[i].focused) {
          element->foregroundColor = Color::parse_color("#E0AAFF", NULL, Color(0xFF, 0xAA, 0xFF, 255));
          element->underlineColor = Color::parse_color("#E0AAFF", NULL, Color(0xFF, 0xAA, 0xFF, 255));
          element->underline = true;
        } else {
          element->foregroundColor = Color::parse_color("#666666", NULL, Color(0x66, 0x66, 0x66, 255));
          element->underline = false;
        }
        
        workspace_elements.push_back(element);
        elements.push_back(element);
      }
      
      free(reply);
    }

    void clear_elements() {
      for (BarElement* element : workspace_elements) {
        elements.erase(std::remove(elements.begin(), elements.end(), element), elements.end());
        delete element;
      }
      workspace_elements.clear();
    }

    void update_elements() {
      I3ipc_reply_workspaces* reply = i3ipc_get_workspaces();
      if (!reply) return;

      // Update existing elements
      for (size_t i = 0; i < workspace_elements.size() && i < (size_t)reply->workspaces_size; ++i) {
        BarElement* element = workspace_elements[i];
        
        // Update content
        element->contentLen = snprintf(element->content, CONTENT_MAX_LEN - 1, 
                                   " %s ", reply->workspaces[i].name);
        element->content[element->contentLen] = '\0';
        element->dirtyContent = true;
        
        // Update styling
        if (reply->workspaces[i].focused) {
          element->foregroundColor = Color::parse_color("#E0AAFF", NULL, Color(0xFF, 0xAA, 0xFF, 255));
          element->underlineColor = Color::parse_color("#E0AAFF", NULL, Color(0xFF, 0xAA, 0xFF, 255));
          element->underline = true;
        } else {
          element->foregroundColor = Color::parse_color("#666666", NULL, Color(0x66, 0x66, 0x66, 255));
          element->underline = false;
        }
      }
      
      // Handle workspace count changes
      if ((size_t)reply->workspaces_size != workspace_elements.size()) {
        initialize_elements();
      }
      
      free(reply);
    }

    void switch_to_workspace(const char* ws_name) {
      // Execute workspace switch
      if (fork() == 0) {
        execlp("i3-msg", "i3-msg", "workspace", ws_name, NULL);
        exit(1);
      }
      
      // Trigger render
      if (renderFunction) {
        renderFunction();
      }
    }

    int subscribe_i3() {
      int pipefd[2];
      if (pipe(pipefd) == -1) return -1;

      pid_t pid = fork();
      if (pid == -1) {
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
      }

      if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        execlp("i3-msg", "i3-msg", "-t", "subscribe", "[\"workspace\"]", NULL);
        exit(1);
      } else {
        close(pipefd[1]);
        int flags = fcntl(pipefd[0], F_GETFL, 0);
        fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK);
        return pipefd[0];
      }
    }

    bool is_workspace_event(const char* json_data) {
      return strstr(json_data, "\"change\":\"focus\"") ||
             strstr(json_data, "\"change\":\"init\"") ||
             strstr(json_data, "\"change\":\"empty\"") ||
             strstr(json_data, "\"change\":\"urgent\"");
    }
};

inline bool WorkspaceModule::handle_i3_events(fd_set &fds) {
  bool workspace_changed = false;

  if (i3_fd != -1 && FD_ISSET(i3_fd, &fds)) {
    char buffer[4096];
    ssize_t bytes_read;
    
    while ((bytes_read = read(i3_fd, buffer, sizeof(buffer) - 1)) > 0) {
      buffer[bytes_read] = '\0';
      
      if (is_workspace_event(buffer)) {
        workspace_changed = true;
        break;
      }
    }

    if (bytes_read == 0) {
      close(i3_fd);
      i3_fd = subscribe_i3();
    }

    if (workspace_changed) {
      update_elements();
    }
  }

  return workspace_changed;
}

#endif // WORKSPACE_H