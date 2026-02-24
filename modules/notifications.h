#ifndef NOTIFICATIONS_H
#define NOTIFICATIONS_H

#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include "module.h"
#include "../barElement.h"

class NotificationsModule : public Module {
private:
    BarElement element;
    bool isPaused = false;
    int waitingCount = 0;

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
            element.foregroundColor = Color::parse_color("#FF6B6B", NULL, Color(255, 107, 107, 255));
        } else {
            snprintf(element.content, CONTENT_MAX_LEN, u8"\U000f009c");
            element.foregroundColor = Color::parse_color("#E0AAFF", NULL, Color(224, 170, 255, 255));
        }
        element.dirtyContent = true;
    }

public:
    NotificationsModule() : Module("notifications", false, 0) {
        element.moduleName = name;

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
        updateState();
        updateVisuals();
    }
};

#endif
