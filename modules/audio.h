#ifndef AUDIO_H
#define AUDIO_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pulse/pulseaudio.h>
#include <pulse/error.h>
#include <vector>
#include <string>

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
    AudioModule() : Module("audio") {
        setUpdatePerIteration(false);
        setSecondsPerUpdate(5);
    }

    ~AudioModule() {
        cleanup_pa();
    }

    bool initialize() override {
        return init_pa();
    }

    void update() override {
        refresh_cache();
        generate_buffer();
    }

    void event(const char* eventValue) override {
        if (strncmp(eventValue, "vol_", 4) == 0) {
            int button = atoi(eventValue + 4);
            handle_audio_click(button);
            refresh_cache();
            generate_buffer();
        }
    }

private:
    pa_mainloop* mainloop = nullptr;
    pa_context* context = nullptr;
    SinkInfo current_sink;
    std::vector<SinkInfo> all_sinks;
    std::string default_sink_name;

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

    void handle_audio_click(int button) {
        pa_operation* o = nullptr;
        pa_cvolume cv;

        switch(button) {
            case 1: // Toggle Mute
                o = pa_context_set_sink_mute_by_index(context, current_sink.index, !current_sink.is_muted, NULL, NULL);
                break;
            case 3: // Next Device (Cycle)
                cycle_sinks();
                break;
            case 4: // Vol +
                pa_cvolume_set(&cv, 1, (pa_volume_t)(std::min((double)PA_VOLUME_MAX, (double)PA_VOLUME_NORM * (current_sink.volume + 2) / 100)));
                o = pa_context_set_sink_volume_by_index(context, current_sink.index, &cv, NULL, NULL);
                break;
            case 5: // Vol -
                pa_cvolume_set(&cv, 1, (pa_volume_t)(std::max(0.0, (double)PA_VOLUME_NORM * (current_sink.volume - 2) / 100)));
                o = pa_context_set_sink_volume_by_index(context, current_sink.index, &cv, NULL, NULL);
                break;
        }

        if (o) {
            while (pa_operation_get_state(o) == PA_OPERATION_RUNNING) pa_mainloop_iterate(mainloop, 1, NULL);
            pa_operation_unref(o);
        }
    }

    //TODO: ver la manera en que se actualice en tiempo real el switch de un dispositivo a otro
    void cycle_sinks() {
        if (all_sinks.size() <= 1) return;
        for (size_t i = 0; i < all_sinks.size(); ++i) {
            if (all_sinks[i].name == current_sink.name) {
                int next = (i + 1) % all_sinks.size();
                pa_operation* o = pa_context_set_default_sink(context, all_sinks[next].name.c_str(), NULL, NULL);
                while (pa_operation_get_state(o) == PA_OPERATION_RUNNING) pa_mainloop_iterate(mainloop, 1, NULL);
                pa_operation_unref(o);
                this->markForUpdate();
                break;
            }
        }
    }

    void generate_buffer() {
        char res[1024];
        const char* icon = get_icon(current_sink.name);

        sprintf(res, "%%{A1:vol_1:}%%{A3:vol_3:}%%{A4:vol_4:}%%{A5:vol_5:}%%{F%s}%s %d%%%%{F-}%%{A}%%{A}%%{A}%%{A}",
                (current_sink.is_muted ? Module::COLOR_MUTED : Module::COLOR_FG),
                icon, current_sink.volume);
        buffer = res;
    }

    const char* get_icon(const std::string& name) {
        if (name.find("bluez") != std::string::npos) return "\uf025";  // Headset
        if (name.find("alsa") != std::string::npos) return "\ue638";   // Speaker
        return "\xef\x90\x9c"; // Generic
    }
};

#endif
