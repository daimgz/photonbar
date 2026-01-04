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
      Module("datetime"),
      dias({"dom", "lun", "mar", "mié", "jue", "vie", "sab"}),
      show_hour(true)
    {
      updateConfiguration();

      baseElement.moduleName = name;
      baseElement.setEvent(BarElement::CLICK_LEFT, [this](){
          this->show_hour = !show_hour;
          updateConfiguration();
          // Forzar actualización inmediata DESPUÉS del clic
          markForUpdate();
        }
      );

      elements.push_back(&baseElement);
  }
  ~DateTimeModule() {
  }

  void update() {
    time_t now = time(NULL);
        struct tm *tm = localtime(&now);

        int len;

        if (show_hour) {
            len = snprintf(
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
            len = snprintf(
                baseElement.content,
                CONTENT_MAX_LEN,
                "%s %02d-%02d-%04d",
                dias[tm->tm_wday],
                tm->tm_mday,
                tm->tm_mon + 1,
                tm->tm_year + 1900
            );
        }

        baseElement.contentLen = (size_t)len;
    }

    void updateConfiguration() {
      if (show_hour) {
        // Mostrar hora: actualizar cada segundo
        setUpdatePerIteration(true);
      } else {
        // Solo fecha: actualizar cada minuto
        setUpdatePerIteration(false);
        setSecondsPerUpdate(60);
      }
    }

    std::array<const char*, 7> dias;
    bool show_hour = false;
};

#endif // DATETIME_H
