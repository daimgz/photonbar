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
  BarElement* label;
  public:
    DateTimeModule():
      Module("datetime"),
      dias({"dom", "lun", "mar", "mié", "jue", "vie", "sab"}),
      show_hour(true)
    {
      updateConfiguration();
      label = new BarElement();
      strcpy(label->content, "Testeo :D\0");
      //label->content = (char*)tmp.c_str();
      label->contentLen = 10;

        std::cout << std::endl << std::endl << label->content << std::endl << std::endl << std::endl;
      elements = {
      label
      };
    }
  ~DateTimeModule() {
    delete label;
  }

    void update() {
      time_t now = time(NULL);
      struct tm *tm = localtime(&now);

      //buffer = "%{A1:dt_click:}";
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
      //buffer += "%{A}";
      strcpy(label->content, (char*)buffer.c_str());
        std::cout << std::endl << std::endl <<  "se actualizo la fecha" <<label->content << std::endl << std::endl << std::endl;
      label->contentLen = buffer.length();
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
