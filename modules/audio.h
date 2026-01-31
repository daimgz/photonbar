#ifndef AUDIO_H
#define AUDIO_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pulse/pulseaudio.h>
#include <pulse/error.h>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <chrono> // Necesario para el control de tiempo

#include "module.h"
#include "../helper.h"

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
    // Mantenemos el estándar de tu barra
    AudioModule() : Module("audio", false, 5) {
        baseElement.moduleName = name;

        baseElement.setEvent(BarElement::CLICK_LEFT, [this]() { toggleMute(); });
        baseElement.setEvent(BarElement::CLICK_RIGHT, [this]() { cycleSinks(); });
        baseElement.setEvent(BarElement::SCROLL_UP, [this]() { adjustVolume(2); });
        baseElement.setEvent(BarElement::SCROLL_DOWN, [this]() { adjustVolume(-2); });

        elements.push_back(&baseElement);
    }

    ~AudioModule() { cleanupPa(); }

    bool initialize() override { return initPa(); }

    void update() override {
        auto now = std::chrono::steady_clock::now();

        // OPTIMIZACIÓN 1: Solo refrescar la info pesada cada 250ms
        // Esto reduce el uso de CPU un 5000% si tu loop es de 5ms
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUpdate).count() < 250) {
            return;
        }

        refreshCache();
        updateElement();
        lastUpdate = now;
    }

private:
    pa_mainloop* mainloop = nullptr;
    pa_context* context = nullptr;
    SinkInfo currentSink;
    std::vector<SinkInfo> allSinks;
    std::string defaultSinkName;
    BarElement baseElement;

    // Variables para control de rendimiento
    std::chrono::steady_clock::time_point lastUpdate;
    std::chrono::steady_clock::time_point lastBatteryCheck;
    int cachedBattery = -1;

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
        // OPTIMIZACIÓN 2: Usar iterate(0) para no bloquear el hilo si no es necesario
        while (pa_operation_get_state(o) == PA_OPERATION_RUNNING) pa_mainloop_iterate(mainloop, 0, NULL);
        pa_operation_unref(o);

        o = pa_context_get_sink_info_list(context, sink_info_callback, this);
        while (pa_operation_get_state(o) == PA_OPERATION_RUNNING) pa_mainloop_iterate(mainloop, 0, NULL);
        pa_operation_unref(o);
    }

    int getBluetoothBatteryLevel(const std::string& sinkName) {
        auto now = std::chrono::steady_clock::now();
        // OPTIMIZACIÓN 3: Cache de batería. Solo ejecutar upower cada 30 segundos.
        // upower es el proceso que consume 32% de tu CPU según el reporte.
        if (cachedBattery != -1 && std::chrono::duration_cast<std::chrono::seconds>(now - lastBatteryCheck).count() < 30) {
            return cachedBattery;
        }

        // Simplificamos el comando para evitar múltiples pipes
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

    // Funciones de interacción se mantienen igual para no romper la lógica
    void toggleMute() {
        pa_operation* o = pa_context_set_sink_mute_by_index(context, currentSink.index, !currentSink.isMuted, NULL, NULL);
        if (o) {
            while (pa_operation_get_state(o) == PA_OPERATION_RUNNING) pa_mainloop_iterate(mainloop, 0, NULL);
            pa_operation_unref(o);
        }
        refreshCache();
        updateElement();
        if (renderFunction) renderFunction();
    }

    void cycleSinks() {
        if (allSinks.size() <= 1) return;
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
        updateElement();
        if (renderFunction) renderFunction();
    }

    void adjustVolume(int delta) {
        int new_volume = std::max(0, currentSink.volume + delta);
        pa_cvolume cv;
        pa_cvolume_set(&cv, 1, (pa_volume_t)((double)PA_VOLUME_NORM * new_volume / 100));
        pa_operation* o = pa_context_set_sink_volume_by_index(context, currentSink.index, &cv, NULL, NULL);
        if (o) {
            while (pa_operation_get_state(o) == PA_OPERATION_RUNNING) pa_mainloop_iterate(mainloop, 0, NULL);
            pa_operation_unref(o);
        }
        refreshCache();
        updateElement();
        if (renderFunction) renderFunction();
    }

    void updateElement() {
        const char* icon = getIcon(currentSink.name);
        if (currentSink.isBluetooth && currentSink.batteryLevel >= 0) {
            baseElement.contentLen = snprintf(baseElement.content, CONTENT_MAX_LEN, "%s %d%% 🔋%s",
                icon, currentSink.volume, Helper::getBatteryIcon(currentSink.batteryLevel));
        } else {
            baseElement.contentLen = snprintf(baseElement.content, CONTENT_MAX_LEN, "%s %d%%",
                icon, currentSink.volume);
        }
        baseElement.content[baseElement.contentLen] = '\0';
        baseElement.dirtyContent = true;
        baseElement.foregroundColor = currentSink.isMuted ?
            Color::parse_color("#FF6B6B", NULL, Color(255, 107, 107, 255)) :
            Color::parse_color("#E0AAFF", NULL, Color(224, 170, 255, 255));
    }

    const char* getIcon(const std::string& name) {
        if (name.find("bluez") != std::string::npos) return u8"\U000f02cb";
        if (name.find("alsa") != std::string::npos) return "\ue638";
        return "\xef\x90\x9c";
    }
};

#endif
