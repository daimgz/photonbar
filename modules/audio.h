#ifndef AUDIO_H
#define AUDIO_H

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
#include <pulse/pulseaudio.h>
#include <pulse/error.h>
#include <algorithm>
#include <cmath>
#include <chrono>

#include "module.h"
#include "../helper.h"
#include "../color_cache.h"
#include "../config.h"

struct SinkInfo {
  std::string name;
  uint32_t index;
  int volume;
  bool isMuted;
  bool isBluetooth;
  int batteryLevel;
};

class AudioModule : public Module {
public:
  AudioModule() : Module("audio", false, 5), pwmonFd(-1), pwmonPid(-1),
    mainloop(nullptr), context(nullptr),
    currentSink(), allSinks(), defaultSinkName(),
    baseElement(), lastUpdate(), lastBatteryCheck(), cachedBattery(-1),
    lastVolume(-1), lastMute(-1), lastSinkName() {
    baseElement.moduleName = name;

    baseElement.setEvent(BarElement::CLICK_LEFT, [this]() { toggleMute(); });
    baseElement.setEvent(BarElement::CLICK_RIGHT, [this]() { cycleSinks(); });
    baseElement.setEvent(BarElement::SCROLL_UP, [this]() { adjustVolume(2); });
    baseElement.setEvent(BarElement::SCROLL_DOWN, [this]() { adjustVolume(-2); });

    elements.push_back(&baseElement);
  }

  ~AudioModule() {
    cleanupPa();
    cleanup();
  }

  bool initialize() override {
    if (!initPa()) return false;
    if (!initializeCommand()) return false;

    refreshCache();
    lastVolume = currentSink.volume;
    lastMute = currentSink.isMuted ? 1 : 0;
    lastSinkName = currentSink.name;

    return true;
  }

  int setupSelectFds(fd_set& fds) override {
    if (pwmonFd < 0) return -1;
    FD_SET(pwmonFd, &fds);
    return pwmonFd;
  }

  bool handleEvents(fd_set& fds) override {
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUpdate).count() < 50) {
        return false;
    }
    if (pwmonFd < 0 || !FD_ISSET(pwmonFd, &fds)) return false;

    char buffer[4096];
    ssize_t n = read(pwmonFd, buffer, sizeof(buffer) - 1);

    if (n <= 0) {
      initializeCommand();
      return false;
    }

    buffer[n] = '\0';

    if ((strstr(buffer, "change") || strstr(buffer, "sink") || strstr(buffer, "sink-input")) ||
         strstr(buffer, "default")) {
      refreshCache();
      updateElement();
      return true;
    }

    return false;
  }

  void update() override {
    auto now = std::chrono::steady_clock::now();

    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUpdate).count() < 250) {
      return;
    }

    refreshCache();
    lastVolume = currentSink.volume;
    lastMute = currentSink.isMuted ? 1 : 0;
    lastSinkName = currentSink.name;
    updateElement();
    lastUpdate = now;
  }

private:
  int pwmonFd;
  pid_t pwmonPid;
  pa_mainloop* mainloop;
  pa_context* context;
  SinkInfo currentSink;
  std::vector<SinkInfo> allSinks;
  std::string defaultSinkName;
  BarElement baseElement;

  std::chrono::steady_clock::time_point lastUpdate;
  std::chrono::steady_clock::time_point lastBatteryCheck;
  int cachedBattery;

  int lastVolume;
  int lastMute;
  std::string lastSinkName;

  bool initPa() {
    mainloop = pa_mainloop_new();
    context = pa_context_new(pa_mainloop_get_api(mainloop), "ModuleAudioContext");
    if (pa_context_connect(context, NULL, PA_CONTEXT_NOFLAGS, NULL) < 0) return false;

    while (true) {
      pa_mainloop_iterate(mainloop, 1, NULL);
      pa_context_state_t state = pa_context_get_state(context);
      if (state == PA_CONTEXT_READY) break;
      if (state == PA_CONTEXT_FAILED || state == PA_CONTEXT_TERMINATED) return false;
    }
    return true;
  }

  void cleanupPa() {
    if (context) { pa_context_disconnect(context); pa_context_unref(context); }
    if (mainloop) { pa_mainloop_free(mainloop); }
  }

  bool initializeCommand() {
    cleanup();

    int pipefd[2];
    if (pipe(pipefd) == -1) return false;

    pwmonPid = fork();
    if (pwmonPid == -1) {
      close(pipefd[0]);
      close(pipefd[1]);
      return false;
    }

    if (pwmonPid == 0) {
      close(pipefd[0]);
      dup2(pipefd[1], STDOUT_FILENO);
      close(pipefd[1]);

      execlp("pactl", "pactl", "subscribe", NULL);
      _exit(1);
    }

    close(pipefd[1]);
    pwmonFd = pipefd[0];

    int flags = fcntl(pwmonFd, F_GETFL, 0);
    fcntl(pwmonFd, F_SETFL, flags | O_NONBLOCK);

    return true;
  }

  void cleanup() {
    if (pwmonFd >= 0) {
      close(pwmonFd);
      pwmonFd = -1;
    }
    if (pwmonPid > 0) {
      kill(pwmonPid, SIGTERM);
      waitpid(pwmonPid, NULL, 0);
      pwmonPid = -1;
    }
  }

  static void sink_info_callback(pa_context *c, const pa_sink_info *i, int eol, void *userdata) {
    if (eol > 0 || !i) return;
    AudioModule* self = static_cast<AudioModule*>(userdata);

    SinkInfo s;
    s.name = i->name;
    s.index = i->index;
    s.isMuted = i->mute;
    s.volume = (int)pa_cvolume_avg(&(i->volume)) * 100 / PA_VOLUME_NORM;
    s.isBluetooth = (s.name.find("bluez") != std::string::npos);

    if (s.isBluetooth) {
      s.batteryLevel = self->getBluetoothBatteryLevel(s.name);
    } else {
      s.batteryLevel = -1;
    }

    self->allSinks.push_back(s);
    if (self->defaultSinkName == s.name) self->currentSink = s;
  }

  static void server_info_callback(pa_context *c, const pa_server_info *i, void *userdata) {
    if (!i) return;
    static_cast<AudioModule*>(userdata)->defaultSinkName = i->default_sink_name;
  }

  void refreshCache() {
    allSinks.clear();
    pa_operation* o = pa_context_get_server_info(context, server_info_callback, this);
    while (pa_operation_get_state(o) == PA_OPERATION_RUNNING) pa_mainloop_iterate(mainloop, 0, NULL);
    pa_operation_unref(o);

    o = pa_context_get_sink_info_list(context, sink_info_callback, this);
    while (pa_operation_get_state(o) == PA_OPERATION_RUNNING) pa_mainloop_iterate(mainloop, 0, NULL);
    pa_operation_unref(o);
  }

  int getBluetoothBatteryLevel(const std::string& sinkName) {
    auto now = std::chrono::steady_clock::now();
    if (cachedBattery != -1 && std::chrono::duration_cast<std::chrono::seconds>(now - lastBatteryCheck).count() < 30) {
      return cachedBattery;
    }

    std::string cmd = "upower -i $(upower -e | grep -E 'bluez|headset|audio' | head -1) 2>/dev/null | grep 'percentage' | awk '{print $2}' | tr -d '%' || echo '-1'";

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return -1;

    char buffer[16];
    if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
      cachedBattery = atoi(buffer);
    }
    pclose(pipe);
    lastBatteryCheck = now;
    return cachedBattery;
  }

  void toggleMute() {
    if (!context || !mainloop) return;
    pa_operation* o = pa_context_set_sink_mute_by_index(context, currentSink.index, !currentSink.isMuted, NULL, NULL);
    if (o) {
      while (pa_operation_get_state(o) == PA_OPERATION_RUNNING) pa_mainloop_iterate(mainloop, 0, NULL);
      pa_operation_unref(o);
    }
    refreshCache();
    lastVolume = currentSink.volume;
    lastMute = currentSink.isMuted ? 1 : 0;
    lastSinkName = currentSink.name;
    //updateElement();
    //if (renderFunction) renderFunction();
  }

  void cycleSinks() {
    if (!context || !mainloop || allSinks.size() <= 1) return;
    for (size_t i = 0; i < allSinks.size(); ++i) {
      if (allSinks[i].name == currentSink.name) {
        int next = (i + 1) % allSinks.size();
        pa_operation* o = pa_context_set_default_sink(context, allSinks[next].name.c_str(), NULL, NULL);
        if (o) {
          while (pa_operation_get_state(o) == PA_OPERATION_RUNNING) pa_mainloop_iterate(mainloop, 0, NULL);
          pa_operation_unref(o);
        }
        break;
      }
    }
    refreshCache();
    lastVolume = currentSink.volume;
    lastMute = currentSink.isMuted ? 1 : 0;
    lastSinkName = currentSink.name;
    //updateElement();
    //if (renderFunction) renderFunction();
  }

  void adjustVolume(int delta) {
    if (!context || !mainloop) return;
    int new_volume = std::max(0, currentSink.volume + delta);
    pa_cvolume cv;
    pa_cvolume_set(&cv, 1, (pa_volume_t)((double)PA_VOLUME_NORM * new_volume / 100));
    pa_operation* o = pa_context_set_sink_volume_by_index(context, currentSink.index, &cv, NULL, NULL);
    if (o) {
      while (pa_operation_get_state(o) == PA_OPERATION_RUNNING) pa_mainloop_iterate(mainloop, 0, NULL);
      pa_operation_unref(o);
    }
    refreshCache();
    lastVolume = currentSink.volume;
    lastMute = currentSink.isMuted ? 1 : 0;
    lastSinkName = currentSink.name;
    //updateElement();
    //if (renderFunction) renderFunction();
  }

  void updateElement() {
    const char* icon = getIcon(currentSink.name);
    if (currentSink.isBluetooth && currentSink.batteryLevel >= 0) {
      baseElement.contentLen = snprintf(baseElement.content, CONTENT_MAX_LEN, "%s %s %d%%",
                                        Helper::getBatteryIcon(currentSink.batteryLevel), icon, currentSink.volume);
    } else {
      baseElement.contentLen = snprintf(baseElement.content, CONTENT_MAX_LEN, "%s %d%%",
                                        icon, currentSink.volume);
    }
    baseElement.content[baseElement.contentLen] = '\0';
    baseElement.dirtyContent = true;
    baseElement.foregroundColor = currentSink.isMuted ?
      ColorCache::lightRed() : ColorCache::purple();
  }

  const char* getIcon(const std::string& name) {
    if (name.find("bluez") != std::string::npos) return u8"\U000f02cb";
    if (name.find("alsa") != std::string::npos) return "\ue638";
    return "\xef\x90\x9c";
  }
};

#endif
