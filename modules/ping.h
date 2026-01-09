#ifndef NETWORK_H
#define NETWORK_H

#include <cstdio>
#include <cstring>
#include <ctime>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "module.h"

class PingModule : public Module {
private:
    bool showDetails = true;
    const char* host = "8.8.8.8";
    int port = 443;

    static constexpr const char* ICON_NET  = "\uef09";
    static constexpr const char* ICON_UP   = "\ueaa0";
    static constexpr const char* ICON_DOWN = "\uea9d";

    unsigned long long lastSent = 0;
    unsigned long long lastRecv = 0;
    double lastUp = 0.0;
    double lastDown = 0.0;

    float cachedLatency = -1.0f;
    time_t lastLatencyCheck = 0;

    BarElement baseElement;

    /* ================== /proc/net/dev ================== */
    int netDevFd = -1;

    static inline unsigned long long fast_atoull(char*& p) {
        unsigned long long val = 0;
        while (*p && (*p < '0' || *p > '9')) ++p;
        while (*p >= '0' && *p <= '9') {
            val = val * 10 + (*p - '0');
            ++p;
        }
        return val;
    }

    inline void openNetDev() {
        if (netDevFd >= 0) return;
        netDevFd = open("/proc/net/dev", O_RDONLY | O_CLOEXEC);
    }

    inline void getNetworkIo(unsigned long long& sent, unsigned long long& recv) {
        if (netDevFd < 0) return;
        if (lseek(netDevFd, 0, SEEK_SET) < 0) return;

        char buf[4096]; // seguro para muchas interfaces
        ssize_t len = read(netDevFd, buf, sizeof(buf) - 1);
        if (len <= 0) return;
        buf[len] = '\0';

        sent = recv = 0;
        char* p = buf;

        // Saltar headers rápidamente
        int nl = 0;
        while (nl < 2 && *p) {
            if (*p++ == '\n') nl++;
        }

        while (*p) {
            while (*p == ' ') ++p;
            if (!*p) break;

            // Filtro seguro de loopback
            if (p[0] == 'l' && p[1] == 'o' && p[2] == ':') {
                while (*p && *p != '\n') ++p;
                if (*p) ++p;
                continue;
            }

            // Saltar nombre de interfaz
            while (*p && *p != ':') ++p;
            if (*p == ':') ++p;

            // Parsing lineal de columnas
            recv += fast_atoull(p);
            for (int i = 0; i < 7; ++i) fast_atoull(p);
            sent += fast_atoull(p);

            // Ir al final de la línea
            while (*p && *p != '\n') ++p;
            if (*p) ++p;
        }
    }

    // TCP no bloqueante + CLOEXEC
    inline float getLatencyMs() {
        int sock = socket(
            AF_INET,
            SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC,
            0
        );
        if (sock < 0) return -1.0f;

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, host, &addr.sin_addr);

        timespec start{}, end{};
        clock_gettime(CLOCK_MONOTONIC_COARSE, &start);

        int r = connect(sock, (sockaddr*)&addr, sizeof(addr));
        if (r < 0 && errno != EINPROGRESS) {
            close(sock);
            return -1.0f;
        }

        pollfd pfd{};
        pfd.fd = sock;
        pfd.events = POLLOUT;

        if (poll(&pfd, 1, 1000) <= 0) {
            close(sock);
            return -1.0f;
        }

        int err = 0;
        socklen_t len = sizeof(err);
        getsockopt(sock, SOL_SOCKET, SO_ERROR, &err, &len);
        if (err) {
            close(sock);
            return -1.0f;
        }

        clock_gettime(CLOCK_MONOTONIC_COARSE, &end);
        close(sock);

        return (end.tv_sec - start.tv_sec) * 1000.0f +
               (end.tv_nsec - start.tv_nsec) / 1e6f;
    }

public:
    PingModule() : Module("network", false, 1) {
        openNetDev();
        getNetworkIo(lastSent, lastRecv);

        baseElement.moduleName = name;
        baseElement.setEvent(
            BarElement::CLICK_LEFT,
            [this]() {
                showDetails = !showDetails;
                update();
                renderFunction();
            }
        );

        elements.push_back(&baseElement);
    }

    ~PingModule() {
        if (netDevFd >= 0)
            close(netDevFd);
    }

    void update() override {
        unsigned long long curSent, curRecv;
        getNetworkIo(curSent, curRecv);

        double up   = (double)(curSent - lastSent) * 0.0009765625;   // KB
        double down = (double)(curRecv - lastRecv) * 0.0009765625;   // KB

        if (up < 0) up = 0;
        if (down < 0) down = 0;

        // Suavizado
        up   = up * 0.7 + lastUp * 0.3;
        down = down * 0.7 + lastDown * 0.3;

        lastUp = up;
        lastDown = down;
        lastSent = curSent;
        lastRecv = curRecv;

        time_t now = time(nullptr);
        if (now != lastLatencyCheck) {
            cachedLatency = getLatencyMs();
            lastLatencyCheck = now;
        }

        if (!showDetails) {
            baseElement.contentLen = snprintf(
                baseElement.content,
                CONTENT_MAX_LEN,
                "%s",
                ICON_NET
            );
        } else if (cachedLatency >= 0.0f) {
            baseElement.contentLen = snprintf(
                baseElement.content,
                CONTENT_MAX_LEN,
                "%s %.1fms %s%.1fK %s%.1fK",
                ICON_NET,
                cachedLatency,
                ICON_UP, up,
                ICON_DOWN, down
            );
        } else {
            baseElement.contentLen = snprintf(
                baseElement.content,
                CONTENT_MAX_LEN,
                "%s Off %s%.1fK %s%.1fK",
                ICON_NET,
                ICON_UP, up,
                ICON_DOWN, down
            );
        }

        baseElement.dirtyContent = true;
        lastUpdate = now;
    }
};

#endif
