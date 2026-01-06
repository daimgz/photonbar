#ifndef DATETIME_H
#define DATETIME_H

#include <cstring>
#include <time.h>
#include <array>
#include <stdio.h>
#include "module.h"
#include <fmt/format.h>

class DateTimeModule : public Module {
  private:
  BarElement baseElement;

  public:

  DateTimeModule():
    Module("datetime", false, 1, ALIGN_R),
    dias({"dom", "lun", "mar", "mié", "jue", "vie", "sab"}),
    show_hour(true)
  {

    baseElement.moduleName = name;
    baseElement.setEvent(
      BarElement::CLICK_LEFT,
      [this](){
        this->show_hour = !show_hour;

        if (this->show_hour) {
          // Mostrar hora: actualizar cada segundo
          this->setSecondsPerUpdate(1);
        } else {
          // Solo fecha: actualizar cada minuto
          this->setSecondsPerUpdate(60);
        }
        update();
        renderFunction();
      }
    );

    elements.push_back(&baseElement);
  }
  ~DateTimeModule() {
  }

  void update() {
        time_t now = time(NULL);
        struct tm *tm = localtime(&now);

        if (show_hour) {
            baseElement.contentLen = snprintf(
                baseElement.content,
                CONTENT_MAX_LEN,
                "%s %02d-%02d-%04d %02d:%02d:%02d",
                dias[tm->tm_wday],
                tm->tm_mday,
                tm->tm_mon + 1,
                tm->tm_year + 1900,
                tm->tm_hour,
                tm->tm_min,
                tm->tm_sec
            );
        } else {
            baseElement.contentLen = snprintf(
                baseElement.content,
                CONTENT_MAX_LEN,
                "%s %02d-%02d-%04d",
                dias[tm->tm_wday],
                tm->tm_mday,
                tm->tm_mon + 1,
                tm->tm_year + 1900
            );
        }
        baseElement.dirtyContent = true;
        lastUpdate = time(nullptr);
        std::cout << "the datetime was updated :D" << std::endl;
    }

    std::array<const char*, 7> dias;
    bool show_hour = false;
};

#endif // DATETIME_H
