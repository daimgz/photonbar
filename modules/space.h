#ifndef SPACE_H
#define SPACE_H

#include <cstring>
#include <ctime>
#include <stdio.h>
#include <stdlib.h>
#include <sys/statvfs.h>
#include <string>
#include "module.h"
#include "../barElement.h"
#include "../color.h"


class SpaceModule : public Module {
  private:
    // Elementos de la UI
    BarElement baseElement;

    // Constantes para modos de visualización
    static const int DISPLAY_FREE = 1;        // Espacio libre en GB
    static const int DISPLAY_USED_PERCENTAGE = 2;  // Porcentaje usado
    static const int NUM_DISPLAY_MODES = 3;   // Total de modos

    // Configuración
    const std::string partition;
    const std::string name;
    int displayMode;

    // Estado del disco
    double freeGb;
    double usedPercentage;
    time_t cacheUntil;

    void getSpaceUsage() {
      struct statvfs vfs;
      if (statvfs(partition.c_str(), &vfs) != 0) {
        fprintf(stderr, "[SpaceModule] Error accessing %s\n", partition.c_str());
        freeGb = 0.0;
        usedPercentage = 0.0;
        cacheUntil = 0;
        return;
      }

      // Calcular espacio libre y usado
      unsigned long block_size = vfs.f_bsize;
      unsigned long free_blocks = vfs.f_bavail;
      unsigned long total_blocks = vfs.f_blocks;

      freeGb = (double)(free_blocks * block_size) / (1024.0 * 1024.0 * 1024.0);
      double used_gb = (double)((total_blocks - free_blocks) * block_size) / (1024.0 * 1024.0 * 1024.0);
      double total_gb = (double)(total_blocks * block_size) / (1024.0 * 1024.0 * 1024.0);

      usedPercentage = (used_gb / total_gb) * 100.0;
      cacheUntil = time(nullptr) + 1; // Cache por 1 segundo
    }



  public:
    SpaceModule():
      Module("space", false, 30),  // No auto-update, cada 30 segundos
      partition("/"),
      name("\uf0c7"),
      displayMode(0)
    {
      // Configurar elemento base
      baseElement.moduleName = name;

      // Click izquierdo: ciclar entre modos de visualización
      baseElement.setEvent(BarElement::CLICK_LEFT, [this]() {
        displayMode = (displayMode + 1) % NUM_DISPLAY_MODES;
        update();
        if (renderFunction) {
          renderFunction();
        }
      });

      // Click derecho: también ciclar entre modos (igual que izquierdo en original)
      baseElement.setEvent(BarElement::CLICK_RIGHT, [this]() {
        displayMode = (displayMode + 1) % NUM_DISPLAY_MODES;
        update();
        if (renderFunction) {
          renderFunction();
        }
      });

      // Color base del texto (igual al COLOR_FG del sistema original)
      baseElement.foregroundColor = Color::parse_color("#E0AAFF", NULL, Color(224, 170, 255, 255));

      elements.push_back(&baseElement);
    }

    void update() override {
      getSpaceUsage();

      // Generar texto según modo actual usando snprintf estándar
      int mode_idx = displayMode % NUM_DISPLAY_MODES;
      
      switch(mode_idx) {
        case DISPLAY_FREE:
          baseElement.contentLen = snprintf(
            baseElement.content,
            CONTENT_MAX_LEN,
            "%s %.2fGB",
            name.c_str(),
            freeGb
          );
          break;
        case DISPLAY_USED_PERCENTAGE:
          baseElement.contentLen = snprintf(
            baseElement.content,
            CONTENT_MAX_LEN,
            "%s %.0f%%",
            name.c_str(),
            usedPercentage
          );
          break;
        default:
          baseElement.contentLen = snprintf(
            baseElement.content,
            CONTENT_MAX_LEN,
            "%s %.2fGB",
            name.c_str(),
            freeGb
          );
          break;
      }

      baseElement.dirtyContent = true;

      // Color fijo igual al original
      baseElement.foregroundColor = Color::parse_color("#E0AAFF", NULL, Color(224, 170, 255, 255));

      // 🔥 CRÍTICO: Actualizar timestamp
      lastUpdate = time(nullptr);
    }
};

#endif // SPACE_H
