// reactor.h - the facade the firmware and the desktop CLI call.
// Units: time s, n normalized power, rho dk/k (rhoDollars in $), T K, period s,
// SUR DPM, rod position 0 = in, 1 = withdrawn.
#ifndef REACTOR_REACTOR_H
#define REACTOR_REACTOR_H

#include <stdint.h>

#include "instruments.h"
#include "kinetics.h"
#include "params.h"

struct Inputs {
  float rodDemand = 0.5f;     // regulating rod demand, 0..1
  float Tcool = 290.0f;       // coolant temperature (K)
  bool scram = false;         // manual trip
  bool reset = false;         // reset request (needs rods and rod demand in)
  bool overrideOn = false;    // physics-test mode: rho = rhoOverride
  float rhoOverride = 0.0f;   // dk/k
};

struct Readings {
  float t, n, rho, rhoDollars, T, period, sur, logP, rod;
  uint8_t trips, warn;
};

class Reactor {
 public:
  // Equilibrium at n = 1, T = Tref, regulating rod at 0.5, safety bank out.
  // Returns false (and keeps the old state) if the plant parameters are invalid.
  bool init(const PlantParams& plant = defaultPlant(), const Setpoints& sp = defaultSetpoints());
  const Readings& step(const Inputs& in, float h);

 private:
  KineticsParams kp_;
  PlantParams pp_;
  Setpoints sp_;
  Kinetics k_;
  PeriodMeter meter_;
  float rod_ = 0.5f;
  float safety_ = 1.0f;
  float dT_ = 0.0f;  // T - Tref (K)
  uint32_t tWhole_ = 0;  // whole seconds; float alone loses 10 ms steps after an hour
  float tFrac_ = 0.0f;
  uint8_t latched_ = 0;
  uint8_t present_ = 0;
  Readings r_;
};

#endif
