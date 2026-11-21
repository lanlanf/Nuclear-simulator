// display.h - SSD1306 128x64 OLED readout.
// Units: power %, rho $, rod %, T K, period s, SUR DPM.
#ifndef DISPLAY_H
#define DISPLAY_H

#include <Adafruit_SSD1306.h>

#include "reactor.h"

class Display {
 public:
  Display() : oled_(128, 64, &Wire, -1) {}
  bool begin();  // false if no OLED answers; draw() then does nothing
  void draw(const Readings& r, bool overrideOn);

 private:
  Adafruit_SSD1306 oled_;
  bool ok_ = false;
};

#endif
