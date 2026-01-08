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
      initialize();
      initializeElements();
    }

    ~WorkspaceModule() {
      clearElements();
    }

    void update() {
      updateElements();
    }

    bool initialize() {
      i3Fd = subscribeI3();
      return i3Fd != -1;
    }

    int setupSelectFds(fd_set &fds) {
      if (i3Fd != -1) {
        FD_SET(i3Fd, &fds);
        return i3Fd;
      }
      return -1;
    }

    bool handleI3Events(fd_set &fds);

  private:
    int i3Fd;
    std::vector<BarElement*> workspaceElements;

    void initializeElements() {
      clearElements();
      
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
          switchToWorkspace(ws_name.c_str());
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
        
        workspaceElements.push_back(element);
        elements.push_back(element);
      }
      
      free(reply);
    }

    void clearElements() {
      for (BarElement* element : workspaceElements) {
        elements.erase(std::remove(elements.begin(), elements.end(), element), elements.end());
        delete element;
      }
      workspaceElements.clear();
    }

    void updateElements() {
      I3ipc_reply_workspaces* reply = i3ipc_get_workspaces();
      if (!reply) return;

      // Update existing elements
      for (size_t i = 0; i < workspaceElements.size() && i < (size_t)reply->workspaces_size; ++i) {
        BarElement* element = workspaceElements[i];
        
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
      if ((size_t)reply->workspaces_size != workspaceElements.size()) {
        initializeElements();
      }
      
      free(reply);
    }

    void switchToWorkspace(const char* ws_name) {
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

    int subscribeI3() {
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

    bool isWorkspaceEvent(const char* json_data) {
      return strstr(json_data, "\"change\":\"focus\"") ||
             strstr(json_data, "\"change\":\"init\"") ||
             strstr(json_data, "\"change\":\"empty\"") ||
             strstr(json_data, "\"change\":\"urgent\"");
    }
};

inline bool WorkspaceModule::handleI3Events(fd_set &fds) {
  bool workspace_changed = false;

  if (i3Fd != -1 && FD_ISSET(i3Fd, &fds)) {
    char buffer[4096];
    ssize_t bytes_read;
    
    while ((bytes_read = read(i3Fd, buffer, sizeof(buffer) - 1)) > 0) {
      buffer[bytes_read] = '\0';
      
      if (isWorkspaceEvent(buffer)) {
        workspace_changed = true;
        break;
      }
    }

    if (bytes_read == 0) {
      close(i3Fd);
      i3Fd = subscribeI3();
    }

    if (workspace_changed) {
      updateElements();
    }
  }

  return workspace_changed;
}

#endif // WORKSPACE_H