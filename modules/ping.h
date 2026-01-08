#ifndef NETWORK_H
#define NETWORK_H

#include <stdio.h>
#include <string.h>
#include <fstream>
#include <iostream>
#include <vector>
#include "module.h"

class PingModule : public Module {
private:
    bool showDetails = true;
    const char* host = "8.8.8.8";

    // Iconos en formato de bytes hexadecimales (UTF-8 seguro)
    static constexpr const char* ICON_NET   = "\uef09"; // 󪩚 (Pulso/Ping)
    static constexpr const char* ICON_DOWN  = "\uea9d"; //  (Flecha abajo)
    static constexpr const char* ICON_UP    = "\ueaa0"; //  (Flecha arriba)

    unsigned long long lastSent = 0;
    unsigned long long lastRecv = 0;

    BarElement baseElement;

    float getPing() {
        char cmd[128];
        snprintf(cmd, sizeof(cmd), "ping -c 1 -W 1 %s | grep 'time=' | awk -F'time=' '{print $2}' | awk '{print $1}'", host);

        FILE* pipe = popen(cmd, "r");
        if (!pipe) return -1.0f;

        char buffer_cmd[32];
        float ping_time = -1.0f;
        if (fgets(buffer_cmd, sizeof(buffer_cmd), pipe) != NULL) {
            ping_time = atof(buffer_cmd);
        }
        pclose(pipe);
        return ping_time;
    }

    void getNetworkIo(unsigned long long &sent, unsigned long long &recv) {
        std::ifstream file("/proc/net/dev");
        std::string line;
        sent = 0; recv = 0;
        std::getline(file, line);
        std::getline(file, line);

        while (std::getline(file, line)) {
            if (line.find("lo:") != std::string::npos) continue;
            unsigned long long r_bytes, t_bytes, tmp;
            char iface[32];
            sscanf(line.c_str(), " %[^:]: %llu %llu %llu %llu %llu %llu %llu %llu %llu",
                   iface, &r_bytes, &tmp, &tmp, &tmp, &tmp, &tmp, &tmp, &tmp, &t_bytes);
            recv += r_bytes;
            sent += t_bytes;
        }
    }

public:
    PingModule() : Module("network", false, 1) {
        getNetworkIo(lastSent, lastRecv);

        baseElement.moduleName = name;
        baseElement.setEvent(
            BarElement::CLICK_LEFT,
            [this](){
                this->showDetails = !this->showDetails;
                update();
                renderFunction();
            }
        );

        elements.push_back(&baseElement);
    }

    void update() {
        unsigned long long current_sent, current_recv;
        getNetworkIo(current_sent, current_recv);
        float ping = getPing();

        double up_speed = (double)(current_sent - lastSent) / 1024.0;
        double down_speed = (double)(current_recv - lastRecv) / 1024.0;

        if (up_speed < 0) up_speed = 0;
        if (down_speed < 0) down_speed = 0;

        lastSent = current_sent;
        lastRecv = current_recv;

        if (!showDetails) {
            baseElement.contentLen = snprintf(
                baseElement.content,
                CONTENT_MAX_LEN,
                "%s",
                ICON_NET
            );
            baseElement.dirtyContent = true;
            lastUpdate = time(nullptr);
            return;
        }

        if (ping >= 0) {
            baseElement.contentLen = snprintf(
                baseElement.content,
                CONTENT_MAX_LEN,
                "%s %.1fms %s%.1fK %s%.1fK",
                ICON_NET, (double)ping, ICON_UP, up_speed, ICON_DOWN, down_speed
            );
        } else {
            baseElement.contentLen = snprintf(
                baseElement.content,
                CONTENT_MAX_LEN,
                "%s Off %s%.1fK %s%.1fK",
                ICON_NET, ICON_UP, up_speed, ICON_DOWN, down_speed
            );
        }
        baseElement.dirtyContent = true;
        lastUpdate = time(nullptr);
        std::cout << "the ping was updated :D" << std::endl;
    }
};

#endif
