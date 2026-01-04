#ifndef MODULE_H
#define MODULE_H

#include <iostream>
#include <array>
#include <vector>
#include <string>
#include <ctime>
#include <algorithm>
#include <cstring>
#include "../barElement.h"

// Forward declaration
class BarManager;

class Module {
  public:

    // Colores comunes para todos los módulos
    static constexpr const char* COLOR_FG = "#E0AAFF";    // Lila claro del tema
    static constexpr const char* COLOR_RED = "#FF6B6B";   // Rojo para estado muteado

    const char *getBuffer() {
      return buffer.c_str();
    }

    const std::string& getName() const {
      return name;
    }

    std::vector<BarElement*> getElements() {
      return elements;
    }

    virtual void update() = 0;
    virtual bool initialize() { return true; } // Default implementation for modules that don't need initialization

    // Método para manejar clics (puede ser sobrescrito)
    virtual bool handleClick(EventFunction func) {
      func();
      if (update_on_click) {
          update();
      }
      return render_on_click;
    }

    // Métodos de control de actualización
    bool shouldUpdate() {
      if (!auto_update) return false;
      if (needs_update) return true;

      if (update_per_iteration) return true;

      time_t now = time(nullptr);
      return (now - last_update) >= seconds_per_update;
    }

    void markForUpdate() { needs_update = true; }
    void setAutoUpdate(bool enabled) { auto_update = enabled; }
    void setUpdatePerIteration(bool enabled) { update_per_iteration = enabled; }
    void setSecondsPerUpdate(int seconds) { seconds_per_update = seconds; }

  protected:
    Module(std::string name):
      update_per_iteration(true),
      seconds_per_update(1),
      last_update(0),
      needs_update(true),
      auto_update(true),
      update_on_click(true),
      render_on_click(true),
      name(name)
    {
    };

    // Configuración de actualización
    bool update_per_iteration;     // ¿Actualizar en cada ciclo?
    int seconds_per_update;        // Intervalo en segundos

    // Estado de actualización
    time_t last_update;            // Timestamp de última actualización
    bool needs_update;             // Forzar actualización
    bool auto_update;              // Control general de actualizaciones

    // Configuración de comportamiento en clics
    bool update_on_click;          // Actualizar instantáneamente en clic
    bool render_on_click;           // Forzar renderizado en clic

    std::string name;
    std::vector<BarElement*> elements;


    //TODO: a eliminar
    std::string buffer ;
    // Permitir acceso a BarManager
    friend class BarManager;
};



#endif // MODULE_H
