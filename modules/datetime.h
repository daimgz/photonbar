#ifndef DATETIME_H
#define DATETIME_H

#include <cstring>
#include <time.h>
#include <array>
#include <stdio.h>
#include "module.h"
#include <fmt/format.h>

class DateTimeModule : public Module {
  public:
    DateTimeModule():
      Module("datetime"),
      dias({"dom", "lun", "mar", "mié", "jue", "vie", "sab"}),
      show_hour(true)
    {
      updateConfiguration();
    }

    void update() {
      time_t now = time(NULL);
      struct tm *tm = localtime(&now);

      buffer = "%{A1:dt_click:}";
      buffer += fmt::format(
        "{} {:02d}-{:02d}-{:04d}",
        dias[tm->tm_wday], tm->tm_mday, tm->tm_mon + 1, tm->tm_year + 1900
      );

      if (show_hour) {
        buffer.append(
          fmt::format(
            " {:02d}:{:02d}:{:02d}",
            tm->tm_hour, tm->tm_min, tm->tm_sec
          )
        );
      }
      buffer += "%{A}";
    }

    void event(const char* eventValue) {
      if (strstr(eventValue, "dt_click")) {
        show_hour = !show_hour;
        updateConfiguration();
        // Forzar actualización inmediata DESPUÉS del clic
        markForUpdate();
      }
    }

  private:
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
