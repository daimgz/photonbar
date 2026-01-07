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

#include "module.h"

// Estructura para almacenar info de los sinks (salidas)
struct SinkInfo {
    std::string name;
    uint32_t index;
    int volume;
    bool is_muted;
};

class AudioModule : public Module {
public:
    AudioModule() : Module("audio", false, 5) {
        // Configurar elemento base
        baseElement.moduleName = name;

        // Click izquierdo: Toggle Mute
        baseElement.setEvent(BarElement::CLICK_LEFT, [this]() {
            toggle_mute();
        });

        // Click medio: Next Device (Cycle)
        baseElement.setEvent(BarElement::CLICK_RIGHT, [this]() {
            cycle_sinks();
        });

        // Scroll up: Volume +
        baseElement.setEvent(BarElement::SCROLL_UP, [this]() {
            adjust_volume(2);
        });

        // Scroll down: Volume -
        baseElement.setEvent(BarElement::SCROLL_DOWN, [this]() {
            adjust_volume(-2);
        });

        elements.push_back(&baseElement);
    }

    ~AudioModule() {
        cleanup_pa();
    }

    bool initialize() override {
        return init_pa();
    }

    void update() override {
        refresh_cache();
        update_element();
    }

private:
    pa_mainloop* mainloop = nullptr;
    pa_context* context = nullptr;
    SinkInfo current_sink;
    std::vector<SinkInfo> all_sinks;
    std::string default_sink_name;
    BarElement baseElement;

    // --- Lógica de PulseAudio ---

    bool init_pa() {
        mainloop = pa_mainloop_new();
        context = pa_context_new(pa_mainloop_get_api(mainloop), "ModuleAudioContext");

        if (pa_context_connect(context, NULL, PA_CONTEXT_NOFLAGS, NULL) < 0) {
            return false;
        }

        // Esperar a que el contexto esté listo
        while (true) {
            pa_mainloop_iterate(mainloop, 1, NULL);
            pa_context_state_t state = pa_context_get_state(context);
            if (state == PA_CONTEXT_READY) break;
            if (state == PA_CONTEXT_FAILED || state == PA_CONTEXT_TERMINATED) return false;
        }
        return true;
    }

    void cleanup_pa() {
        if (context) { pa_context_disconnect(context); pa_context_unref(context); }
        if (mainloop) { pa_mainloop_free(mainloop); }
    }

    // Callback para recibir información de los sinks
    static void sink_info_callback(pa_context *c, const pa_sink_info *i, int eol, void *userdata) {
        if (eol > 0 || !i) return;
        AudioModule* self = static_cast<AudioModule*>(userdata);

        SinkInfo s;
        s.name = i->name;
        s.index = i->index;
        s.is_muted = i->mute;
        s.volume = (int)pa_cvolume_avg(&(i->volume)) * 100 / PA_VOLUME_NORM;

        self->all_sinks.push_back(s);

        // Si este es el sink por defecto, actualizar current_sink
        if (self->default_sink_name == s.name) {
            self->current_sink = s;
        }
    }

    static void server_info_callback(pa_context *c, const pa_server_info *i, void *userdata) {
        if (!i) return;
        AudioModule* self = static_cast<AudioModule*>(userdata);
        self->default_sink_name = i->default_sink_name;
    }

    void refresh_cache() {
        all_sinks.clear();

        // 1. Obtener nombre del sink por defecto
        pa_operation* o = pa_context_get_server_info(context, server_info_callback, this);
        while (pa_operation_get_state(o) == PA_OPERATION_RUNNING) pa_mainloop_iterate(mainloop, 1, NULL);
        pa_operation_unref(o);

        // 2. Obtener lista de todos los sinks
        o = pa_context_get_sink_info_list(context, sink_info_callback, this);
        while (pa_operation_get_state(o) == PA_OPERATION_RUNNING) pa_mainloop_iterate(mainloop, 1, NULL);
        pa_operation_unref(o);
    }

    void toggle_mute() {
        pa_operation* o = pa_context_set_sink_mute_by_index(context, current_sink.index, !current_sink.is_muted, NULL, NULL);
        if (o) {
            while (pa_operation_get_state(o) == PA_OPERATION_RUNNING) pa_mainloop_iterate(mainloop, 1, NULL);
            pa_operation_unref(o);
        }
        refresh_cache();
        update_element();
        if (renderFunction) {
            renderFunction();
        }
    }

    void cycle_sinks() {
        if (all_sinks.size() <= 1) return;

        for (size_t i = 0; i < all_sinks.size(); ++i) {
            if (all_sinks[i].name == current_sink.name) {
                int next = (i + 1) % all_sinks.size();
                pa_operation* o = pa_context_set_default_sink(context, all_sinks[next].name.c_str(), NULL, NULL);
                if (o) {
                    while (pa_operation_get_state(o) == PA_OPERATION_RUNNING) pa_mainloop_iterate(mainloop, 1, NULL);
                    pa_operation_unref(o);
                }
                break;
            }
        }
        refresh_cache();
        update_element();
        if (renderFunction) {
            renderFunction();
        }
    }

    void adjust_volume(int delta) {
        pa_operation* o = nullptr;
        pa_cvolume cv;

        int new_volume = std::max(0, current_sink.volume + delta);
        pa_cvolume_set(&cv, 1, (pa_volume_t)((double)PA_VOLUME_NORM * new_volume / 100));

        o = pa_context_set_sink_volume_by_index(context, current_sink.index, &cv, NULL, NULL);
        if (o) {
            while (pa_operation_get_state(o) == PA_OPERATION_RUNNING) pa_mainloop_iterate(mainloop, 1, NULL);
            pa_operation_unref(o);
        }
        refresh_cache();
        update_element();
        if (renderFunction) {
            renderFunction();
        }
    }

    void update_element() {
        const char* icon = get_icon(current_sink.name);

        // Actualizar contenido del elemento
        baseElement.contentLen = snprintf(
            baseElement.content,
            CONTENT_MAX_LEN,
            "%s %d%%",
            icon,
            current_sink.volume
        );
        baseElement.content[baseElement.contentLen] = '\0';
        baseElement.dirtyContent = true;

        // Actualizar color según estado de mute
        if (current_sink.is_muted) {
            baseElement.foregroundColor = Color::parse_color("#FF6B6B", NULL, Color(255, 107, 107, 255)); // Rojo
        } else {
            baseElement.foregroundColor = Color::parse_color("#E0AAFF", NULL, Color(224, 170, 255, 255)); // Morado
        }
    }

    const char* get_icon(const std::string& name) {
        if (name.find("bluez") != std::string::npos) return u8"\U000f02cb";  // Headset
        if (name.find("alsa") != std::string::npos) return "\ue638";       // Speaker
        return "\xef\x90\x9c"; // Generic
    }
};

#endif // AUDIO_H
