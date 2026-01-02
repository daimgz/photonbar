#ifndef SPACE_H
#define SPACE_H

#include <cstring>
#include <ctime>
#include <stdio.h>
#include <stdlib.h>
#include <sys/statvfs.h>
#include <string>
#include "module.h"
#include <fmt/format.h>

class SpaceModule : public Module {
  public:
    SpaceModule():
      Module("space"),
      partition("/"),
      name("\uf0c7"),
      display_mode(0) {
      // Configurar actualización cada 30 segundos
      setSecondsPerUpdate(30);
      update_per_iteration = false;
    }

    void update() override {
      getSpaceUsage();
      generateBuffer();
    }

    void event(const char* eventValue) override {
      if (strstr(eventValue, "sp_click")) {
        if (strstr(eventValue, "right")) {
          // Click derecho: ciclar entre modos de visualización
          display_mode = (display_mode + 1) % NUM_DISPLAY_MODES;
        } else {
          // Click izquierdo: alternar modo
          display_mode = (display_mode + 1) % NUM_DISPLAY_MODES;
        }
        markForUpdate();
      }
    }

  private:
    // Constantes para modos de visualización
    static const int DISPLAY_FREE = 1;        // Espacio libre en GB
    static const int DISPLAY_USED_PERCENTAGE = 2;  // Porcentaje usado
    static const int NUM_DISPLAY_MODES = 3;   // Total de modos

    // Configuración
    const std::string partition;
    const std::string name;
    int display_mode;

    // Estado del disco
    double free_gb;
    double used_percentage;
    time_t cache_until;

    // Textos de visualización
    std::string display_texts[NUM_DISPLAY_MODES];

    void getSpaceUsage() {
      struct statvfs vfs;
      if (statvfs(partition.c_str(), &vfs) != 0) {
        fprintf(stderr, "[SpaceModule] Error accessing %s\n", partition.c_str());
        free_gb = 0.0;
        used_percentage = 0.0;
        cache_until = 0;
        return;
      }

      // Calcular espacio libre y usado
      unsigned long block_size = vfs.f_bsize;
      unsigned long free_blocks = vfs.f_bavail;
      unsigned long total_blocks = vfs.f_blocks;

      free_gb = (double)(free_blocks * block_size) / (1024.0 * 1024.0 * 1024.0);
      double used_gb = (double)((total_blocks - free_blocks) * block_size) / (1024.0 * 1024.0 * 1024.0);
      double total_gb = (double)(total_blocks * block_size) / (1024.0 * 1024.0 * 1024.0);

      used_percentage = (used_gb / total_gb) * 100.0;
      cache_until = time(nullptr) + 1; // Cache por 1 segundo
    }

    std::string getDisplayText(int mode) {
      switch(mode) {
        case DISPLAY_FREE:
          return fmt::format("{} {:.2f}GB", name, free_gb);
        case DISPLAY_USED_PERCENTAGE:
          return fmt::format("{} {:.0f}%", name, used_percentage);
        default:
          return fmt::format("{} {:.2f}GB", name, free_gb);
      }
    }

    void generateBuffer() {
      // Construir textos de visualización
      display_texts[DISPLAY_FREE - 1] = getDisplayText(DISPLAY_FREE);
      display_texts[DISPLAY_USED_PERCENTAGE - 1] = getDisplayText(DISPLAY_USED_PERCENTAGE);

      // Seleccionar texto según modo actual
      int mode_idx = display_mode % NUM_DISPLAY_MODES;
      std::string display_text = display_texts[mode_idx];

      // Formato final para la barra
      buffer = "%{A1:sp_click:}";

      // Si es modo de espacio libre (show cache indicator)
      if (display_mode % NUM_DISPLAY_MODES == 0) {
        buffer += display_text + " %{F-}";
      } else {
        buffer += display_text;
      }

      buffer += "%{A}";
    }
};

#endif // SPACE_H
