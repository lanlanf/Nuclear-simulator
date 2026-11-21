// panel_io.h - front panel: rod and coolant pots, SCRAM/reset buttons, LEDs,
// buzzer and the rod-position servo.
// Units: rod demand 0..1 (1 = withdrawn), coolant temperature K.
#ifndef PANEL_IO_H
#define PANEL_IO_H

#include <Arduino.h>
#include <Servo.h>

#include "reactor.h"

class Panel {
 public:
  void begin();
  // Pots plus one-shot SCRAM and reset events since the last call.
  Inputs readInputs();
  // Silences the trip buzzer until a new trip.
  void acknowledge() { acked_ = true; }
  // LEDs, buzzer and servo.
  void show(const Readings& r);

 private:
  Servo servo_;
  float rodPot_ = 0.5f;
  float coolPot_ = 0.5f;
  bool resetRaw_ = false;
  bool resetStable_ = false;
  uint32_t resetChangedMs_ = 0;
  bool acked_ = false;
  uint8_t lastTrips_ = 0;
  int lastAngle_ = -10;
};

#endif
