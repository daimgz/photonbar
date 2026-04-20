#ifndef NOTIFICATIONS_H
#define NOTIFICATIONS_H

#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <chrono>
#include "module.h"
#include "../barElement.h"


class NotificationsModule : public Module {
private:
    BarElement element;
    bool isPaused = false;
    int waitingCount = 0;

    std::chrono::steady_clock::time_point lastStateUpdate;
    static constexpr int CACHE_DURATION_SECONDS = 30;

    void updateState() {
        FILE* f = popen("dunstctl is-paused", "r");
        if (f) {
            char buf[16] = {};
            fgets(buf, sizeof(buf), f);
            isPaused = (strcmp(buf, "true\n") == 0 || strcmp(buf, "true") == 0);
            pclose(f);
        }

        f = popen("dunstctl count waiting", "r");
        if (f) {
            char buf[16] = {};
            fgets(buf, sizeof(buf), f);
            waitingCount = atoi(buf);
            pclose(f);
        }

        lastStateUpdate = std::chrono::steady_clock::now();
    }

    void toggleNotifications() {
        system("dunstctl set-paused toggle");
        updateState();
        updateVisuals();
    }

    void updateVisuals() {
        if (isPaused) {
            if (waitingCount > 0) {
                snprintf(element.content, CONTENT_MAX_LEN, u8"\U000f009b %d", waitingCount);
            } else {
                snprintf(element.content, CONTENT_MAX_LEN, u8"\U000f009b");
            }
            element.foregroundColor = Color::RED;
        } else {
            snprintf(element.content, CONTENT_MAX_LEN, u8"\U000f009c");
            element.foregroundColor = Config::COLOR_FOREGROUND;
        }
        element.dirtyContent = true;
    }

    bool needsUpdate() {
        auto now = std::chrono::steady_clock::now();
        int elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastStateUpdate).count();
        return elapsed >= CACHE_DURATION_SECONDS;
    }

public:
    NotificationsModule() : Module("notifications", false, 5) {
        element.moduleName = name;
        lastStateUpdate = std::chrono::steady_clock::now();

        element.setEvent(BarElement::CLICK_LEFT, [this]() {
            toggleNotifications();
            if (renderFunction) {
                renderFunction();
            }
        });

        elements.push_back(&element);
    }

    bool initialize() override {
        updateState();
        updateVisuals();
        return true;
    }

    void update() override {
        if (needsUpdate()) {
            updateState();
        }
        updateVisuals();
    }
};

#endif
