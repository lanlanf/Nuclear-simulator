// display.cpp - SSD1306 128x64 OLED readout, five text lines.
// Units: power %, rho $, rod %, T K, period s, SUR DPM.
#include "display.h"

#include <math.h>

#include "protection.h"

static const uint8_t OLED_ADDRESS = 0x3C;

static const char* stateText(const Readings& r, bool overrideOn) {
  if (r.trips & TRIP_FAULT) return "TRIP: FAULT";
  if (r.trips & TRIP_PROMPT_CRIT) return "TRIP: PROMPT CRIT";
  if (r.trips & TRIP_SHORT_PERIOD) return "TRIP: SHORT PERIOD";
  if (r.trips & TRIP_HIGH_POWER) return "TRIP: HIGH POWER";
  if (r.trips & TRIP_HIGH_TEMP) return "TRIP: HIGH TEMP";
  if (r.trips & TRIP_MANUAL) return "TRIP: MANUAL SCRAM";
  if (overrideOn) return "TEST: RHO OVERRIDE";
  if (r.warn) return "WARNING";
  if (r.rho < -5.0e-5f) return "SUBCRITICAL";
  if (r.rho > 5.0e-5f) return "SUPERCRITICAL";
  return "CRITICAL";
}

bool Display::begin() {
  ok_ = oled_.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS);
  return ok_;
}

void Display::draw(const Readings& r, bool overrideOn) {
  if (!ok_) return;
  oled_.clearDisplay();
  oled_.setTextSize(1);
  oled_.setTextColor(SSD1306_WHITE);

  oled_.setCursor(0, 0);
  oled_.print("P ");
  oled_.print(100.0f * r.n, 1);
  oled_.print("%  log ");
  oled_.print(r.logP, 2);

  oled_.setCursor(0, 12);
  oled_.print("rho ");
  if (r.rhoDollars >= 0.0f) oled_.print('+');
  oled_.print(r.rhoDollars, 2);
  oled_.print("$ rod ");
  oled_.print(static_cast<int>(100.0f * r.rod + 0.5f));
  oled_.print('%');

  oled_.setCursor(0, 24);
  oled_.print("T ");
  oled_.print(r.T, 1);
  oled_.print("K per ");
  if (fabsf(r.period) >= 1.0e4f) oled_.print("inf");
  else oled_.print(static_cast<int>(r.period));
  oled_.print('s');

  oled_.setCursor(0, 36);
  oled_.print("SUR ");
  oled_.print(r.sur, 2);
  oled_.print(" DPM");

  // Line 5 is inverted while tripped. 21 characters fit at text size 1.
  if (r.trips) {
    oled_.fillRect(0, 50, 128, 14, SSD1306_WHITE);
    oled_.setTextColor(SSD1306_BLACK);
  }
  oled_.setCursor(2, 53);
  oled_.print(stateText(r, overrideOn));
  oled_.display();
}
