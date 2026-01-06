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
    static constexpr const char* COLOR_RED = "#FF6B6B";   // Rojo para estado muteado

    xcb_window_t window;
    //TODO: cambiar a mejor tipo
    int alignment;           // ALIGN_L/C/R
    int fontIndex;          // -1 = automático
    int screenTarget;       // +/-/f/l/número

    // TODO:a eliimnars
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

    //TODO: a eliminar
    virtual void handleClick(EventFunction func) {
      func();
      this->renderFunction();
      //if (update_on_click) {
          //update();
      //}
      //return render_on_click;
    }

    // Métodos de control de actualización
  //
    bool shouldUpdate() {
      if (autoUpdate) return true; // Actulizar por cada render
      std::cout << "llegamos al should update" << std::endl;
      time_t now = time(nullptr);
      std::cout << "now: " << now << " lastUpdate: " << lastUpdate << " secondsPerUpdate: " << secondsPerUpdate << std::endl;
      return (now - lastUpdate) >= secondsPerUpdate;
    }

    bool checkAndUpdate() {
      std::cout << "llega a preguntar si se puede actualizar" << std::endl;
      if (shouldUpdate()) {
        std::cout << "se actualiza el modulo " << name << std::endl;
        update();
        return true;
      }
      return false;
    }

    void setRenderFunction(std::function<void()> renderFunction) {
      this->renderFunction = renderFunction;
    }


  protected:
    void setAutoUpdate(bool enabled) { autoUpdate = enabled; }
    void setSecondsPerUpdate(int seconds) { secondsPerUpdate = seconds; }

    Module(std::string name, bool autoUpdate, int secondsPerUpdate, int alignment) :
      alignment(alignment),
      fontIndex(-1),
      screenTarget(0),
      secondsPerUpdate(secondsPerUpdate),
      lastUpdate(0),
      autoUpdate(autoUpdate),
      name(name)
    {
    };
    std::function<void()> renderFunction;
    // Configuración de actualización
    bool update_per_iteration;     // ¿Actualizar en cada ciclo?
    int secondsPerUpdate;        // Intervalo en segundos

    // Estado de actualización
    time_t lastUpdate;            // Timestamp de última actualización
    bool needs_update;             // Forzar actualización
    bool autoUpdate;              // Control general de actualizaciones

    std::string name;
    std::vector<BarElement*> elements;


    //TODO: a eliminar
    std::string buffer ;
    // Permitir acceso a BarManager
    friend class BarManager;
};



#endif // MODULE_H
