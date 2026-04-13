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

#include "i3ipc.h"
#include "module.h"
#include "../color_cache.h"
#include "../config.h"

class WorkspaceModule : public Module {
public:
  WorkspaceModule() : Module("workspace", true, 1), i3Fd(-1) {
    initialize();
    initializeElements();
  }

  ~WorkspaceModule() {
    clearElements();
    if (i3Fd >= 0) close(i3Fd);
  }

  void update() override {
    updateElements();
  }

  bool initialize() override {
    i3Fd = subscribeI3();
    return i3Fd >= 0;
  }

  int setupSelectFds(fd_set &fds) override {
    if (i3Fd != -1) {
      FD_SET(i3Fd, &fds);
      return i3Fd;
    }
    return -1;
  }

  bool handleEvents(fd_set &fds) override {
    if (i3Fd == -1) {
      i3Fd = subscribeI3();
      return false;
    }

    if (!FD_ISSET(i3Fd, &fds)) return false;

    char buffer[4096];
    ssize_t bytes_read = read(i3Fd, buffer, sizeof(buffer) - 1);

    if (bytes_read <= 0) {
      close(i3Fd);
      i3Fd = subscribeI3();
      return false;
    }

    buffer[bytes_read] = '\0';

    if (strstr(buffer, "\"change\":\"focus\"") ||
      strstr(buffer, "\"change\":\"init\"") ||
      strstr(buffer, "\"change\":\"empty\"") ||
      strstr(buffer, "\"change\":\"urgent\"")) {
      updateElements();
      return true;
    }
    return false;
  }

private:
  int i3Fd;
  std::vector<BarElement*> workspaceElements;

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
    }

    close(pipefd[1]);
    int flags = fcntl(pipefd[0], F_GETFL, 0);
    fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK);
    return pipefd[0];
  }

  void initializeElements() {
    clearElements();

    I3ipc_reply_workspaces* reply = i3ipc_get_workspaces();
    if (!reply) return;

    for (int i = 0; i < reply->workspaces_size; ++i) {
      BarElement* element = new BarElement();
      element->moduleName = name;

      element->contentLen = snprintf(element->content, CONTENT_MAX_LEN - 1,
                                     " %s ", reply->workspaces[i].name);
      element->content[element->contentLen] = '\0';
      element->dirtyContent = true;

      std::string ws_name = reply->workspaces[i].name;
      element->setEvent(BarElement::CLICK_LEFT, [this, ws_name]() {
        if (fork() == 0) {
          execlp("i3-msg", "i3-msg", "workspace", ws_name.c_str(), NULL);
          exit(1);
        }
        if (renderFunction) renderFunction();
      });

      if (reply->workspaces[i].focused) {
        element->foregroundColor = ColorCache::purple();
        element->underlineColor = ColorCache::purple();
        element->underline = true;
      } else {
        element->foregroundColor = ColorCache::gray();
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

    for (size_t i = 0; i < workspaceElements.size() && i < (size_t)reply->workspaces_size; ++i) {
      BarElement* element = workspaceElements[i];

      element->contentLen = snprintf(element->content, CONTENT_MAX_LEN - 1,
                                     " %s ", reply->workspaces[i].name);
      element->content[element->contentLen] = '\0';
      element->dirtyContent = true;

      if (reply->workspaces[i].focused) {
        element->foregroundColor = ColorCache::purple();
        element->underlineColor = ColorCache::purple();
        element->underline = true;
      } else {
        element->foregroundColor = ColorCache::gray();
        element->underline = false;
      }
    }

    if ((size_t)reply->workspaces_size != workspaceElements.size()) {
      initializeElements();
    }

    free(reply);
  }
};

#endif
