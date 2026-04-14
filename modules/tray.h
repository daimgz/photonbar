#ifndef TRAY_H
#define TRAY_H

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <unistd.h>
#include <vector>
#include <string>



#include "module.h"
#include "../color_cache.h"

class TrayModule : public Module {
public:
  TrayModule() : Module("tray", false, 60) {
    iconElement.moduleName = name;
    elements.push_back(&iconElement);

    // Click left → toggle mute
    iconElement.setEvent(BarElement::CLICK_LEFT, []() {
      system("wpctl set-mute @DEFAULT_AUDIO_SINK@ toggle 2>/dev/null");
    });

    update();
  }

  void update() override {
    iconElement.contentLen = snprintf(iconElement.content, CONTENT_MAX_LEN, " ");
    iconElement.content[iconElement.contentLen] = '\0';
    iconElement.dirtyContent = true;
    iconElement.foregroundColor = ColorCache::purple();

    lastUpdate = time(nullptr);
  }

private:
  BarElement iconElement;
};

#endif
