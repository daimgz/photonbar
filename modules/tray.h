#ifndef TRAY_H
#define TRAY_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <vector>
#include <string>

#include "module.h"
#include "../helper.h"
#include "../color_cache.h"
#include "../config.h"

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
    char buffer[256];
    std::string icons;

    // Volume
    FILE* fp = popen("wpctl get-volume @DEFAULT_AUDIO_SINK@ 2>/dev/null", "r");
    if (fp) {
      if (fgets(buffer, sizeof(buffer), fp)) {
        if (strstr(buffer, "Muted: yes")) {
          icons += u8"\U000f1f507 ";
        } else {
          icons += u8"\U000f027a ";
        }
      }
      pclose(fp);
    }

    // Network
    fp = popen("nmcli -t -f STATE general 2>/dev/null", "r");
    if (fp) {
      if (fgets(buffer, sizeof(buffer), fp)) {
        if (strstr(buffer, "connected")) {
          icons += u8"\U000f0650 ";
        } else {
          icons += u8"\U000f0712 ";
        }
      }
      pclose(fp);
    }

    // Bluetooth
    fp = popen("bluetoothctl show 2>/dev/null | grep 'Powered:' | grep -o 'yes'", "r");
    if (fp) {
      if (fgets(buffer, sizeof(buffer), fp)) {
        if (strstr(buffer, "yes")) {
          icons += u8"\U000f02cb ";
        }
      }
      pclose(fp);
    }

    // Battery
    fp = popen("cat /sys/class/power_supply/BAT0/capacity 2>/dev/null", "r");
    if (fp) {
      if (fgets(buffer, sizeof(buffer), fp)) {
        int battery = atoi(buffer);
        char status[16];
        FILE* sf = popen("cat /sys/class/power_supply/BAT0/status 2>/dev/null", "r");
        if (sf) {
          if (fscanf(sf, "%15s", status) == 1) {
            bool charging = (status[0] == 'C');
            icons += Helper::getBatteryIcon(battery, charging);
          }
          pclose(sf);
        }
      }
      pclose(fp);
    }

    if (icons.empty()) {
      icons = "  ";
    }

    iconElement.contentLen = snprintf(iconElement.content, CONTENT_MAX_LEN, "%s", icons.c_str());
    iconElement.content[iconElement.contentLen] = '\0';
    iconElement.dirtyContent = true;
    iconElement.foregroundColor = ColorCache::purple();

    lastUpdate = time(nullptr);
  }

private:
  BarElement iconElement;
};

#endif
