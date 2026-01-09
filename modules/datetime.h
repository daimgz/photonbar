#ifndef DATETIME_H
#define DATETIME_H

#include <ctime>
#include <array>
#include <cstring>
#include "module.h"

class DateTimeModule : public Module {
private:
    BarElement baseElement;
    const char* dias[7] = {"dom","lun","mar","mié","jue","vie","sab"};
    bool showHour = true;

    char contentBuf[32]{};  // buffer fijo
    int lastSec = -1;
    int lastMin = -1;
    int lastHour = -1;
    int lastMday = -1;
    int lastWday = -1;
    int lastMonth = -1;
    int lastYear = -1;

    // Escribe un número de 2 dígitos en el buffer
    inline void write2digits(char* dest, int val) {
        dest[0] = '0' + val / 10;
        dest[1] = '0' + val % 10;
    }

public:
    DateTimeModule() : Module("datetime", false, 1) {
        baseElement.moduleName = name;
        baseElement.setEvent(
            BarElement::CLICK_LEFT,
            [this]() {
                showHour = !showHour;
                setSecondsPerUpdate(showHour ? 1 : 60);
                update();
                renderFunction();
            }
        );
        elements.push_back(&baseElement);
    }

    void update() override {
        std::time_t now = std::time(nullptr);
        std::tm tm{};
        localtime_r(&now, &tm);  // thread-safe

        // Solo actualizar si cambió algo
        if (tm.tm_sec == lastSec && tm.tm_min == lastMin &&
            tm.tm_hour == lastHour && tm.tm_mday == lastMday &&
            tm.tm_wday == lastWday && tm.tm_mon == lastMonth &&
            tm.tm_year == lastYear) {
            return;
        }

        lastSec = tm.tm_sec;
        lastMin = tm.tm_min;
        lastHour = tm.tm_hour;
        lastMday = tm.tm_mday;
        lastWday = tm.tm_wday;
        lastMonth = tm.tm_mon;
        lastYear = tm.tm_year;

        char* p = contentBuf;
        const char* dayStr = dias[tm.tm_wday];

        // Día
        std::memcpy(p, dayStr, 3);
        p += 3;
        *p++ = ' ';

        // Fecha
        write2digits(p, tm.tm_mday); p += 2;
        *p++ = '-';
        write2digits(p, tm.tm_mon + 1); p += 2;
        *p++ = '-';
        int year = tm.tm_year + 1900;
        *p++ = '0' + year / 1000; year %= 1000;
        *p++ = '0' + year / 100;  year %= 100;
        *p++ = '0' + year / 10;   year %= 10;
        *p++ = '0' + year;

        if (showHour) {
            *p++ = ' ';
            write2digits(p, tm.tm_hour); p += 2;
            *p++ = ':';
            write2digits(p, tm.tm_min); p += 2;
            *p++ = ':';
            write2digits(p, tm.tm_sec); p += 2;
        }

        baseElement.contentLen = p - contentBuf;
        std::memcpy(baseElement.content, contentBuf, baseElement.contentLen);
        baseElement.dirtyContent = true;
        lastUpdate = now;
    }
};

#endif // DATETIME_H
