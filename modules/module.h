#ifndef MODULE_H
#define MODULE_H

#include <iostream>
#include <array>
#include <vector>
#include <string>
#include <ctime>
#include <algorithm>
#include <cstring>
#include <sys/select.h>
#include "../barElement.h"
#include "../config.h"

// Forward declaration
class BarManager;

class Module {
  public:

    xcb_window_t window;
    int fontIndex;          // -1 = automático
    int screenTarget;       // +/-/f/l/número

    const std::string& getName() const {
      return name;
    }

    std::vector<BarElement*> getElements() {
      return elements;
    }

    virtual void update() = 0;
    virtual bool initialize() { return true; }

    // Métodos opcionales para manejo de eventos asíncronos
    // Retorna -1 si no hay file descriptor (default)
    // Retorna el fd si el módulo necesita ser escuchado en select()
    virtual int setupSelectFds(fd_set& fds) {
        (void)fds;
        return -1;
    }

    // Maneja eventos disponibles en los fds registrados
    // Retorna true si el módulo necesita actualizarse
    virtual bool handleEvents(fd_set& fds) {
        (void)fds;
        return false;
    }

    // Métodos de control de actualización
  //
    virtual bool shouldUpdate() {
      if (autoUpdate) return true;
      if (secondsPerUpdate <= 0) return false;
      time_t now = time(nullptr);
      return (now - lastUpdate) >= secondsPerUpdate;
    }

    bool checkAndUpdate() {
      if (shouldUpdate()) {
        #if DEBUG
          std::cout << "shouldUpdate " << name << std::endl;
        #endif

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

    Module(std::string name, bool autoUpdate, int secondsPerUpdate) :
      fontIndex(-1),
      screenTarget(0),
      secondsPerUpdate(secondsPerUpdate),
      lastUpdate(time(nullptr)),
      autoUpdate(autoUpdate),
      name(name)
    {
    };
    std::function<void()> renderFunction;
    // Configuración de actualización
    bool updatePerIteration;     // ¿Actualizar en cada ciclo?
    int secondsPerUpdate;        // Intervalo en segundos

    // Estado de actualización
    time_t lastUpdate;            // Timestamp de última actualización
    bool needsUpdate;             // Forzar actualización
    bool autoUpdate;              // Control general de actualizaciones

    std::string name;
    std::vector<BarElement*> elements;

    // Permitir acceso a BarManager
    friend class BarManager;
};



#endif // MODULE_H
