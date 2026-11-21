// reactor.cpp - one fixed time step of the whole plant.
// Units: time s, n normalized power, rho dk/k, T K, period s.
#include "reactor.h"

#include <math.h>

#include "plant.h"
#include "protection.h"

// Override is a physics-test mode, so these trips are bypassed while it is on.
static const uint8_t OVERRIDE_BYPASS = TRIP_HIGH_POWER | TRIP_HIGH_TEMP | TRIP_SHORT_PERIOD;

bool Reactor::init(const PlantParams& plant, const Setpoints& sp) {
  if (!isValid(plant)) return false;
  kp_ = keepinU235();
  pp_ = plant;
  sp_ = sp;
  initEquilibrium(k_, kp_, 1.0f);
  initPeriodMeter(meter_);
  rod_ = 0.5f;
  safety_ = 1.0f;
  dT_ = 0.0f;
  tWhole_ = 0;
  tFrac_ = 0.0f;
  latched_ = 0;
  present_ = 0;
  r_ = Readings();
  return true;
}

const Readings& Reactor::step(const Inputs& in, float h) {
  if (in.scram) latched_ |= TRIP_MANUAL;
  if (in.reset && latched_ != 0) {
    // The rod pot must be in too, or the rod would run out on its own after reset.
    tryReset(latched_, rod_ > in.rodDemand ? rod_ : in.rodDemand, present_);
  }

  // 1. Rods: both banks in at scram speed while tripped.
  if (latched_ != 0) {
    rod_ = moveRod(rod_, 0.0f, pp_.scramSpeed, h);
    safety_ = moveRod(safety_, 0.0f, pp_.scramSpeed, h);
  } else {
    rod_ = moveRod(rod_, in.rodDemand, pp_.rodSpeed, h);
    safety_ = moveRod(safety_, 1.0f, pp_.rodSpeed, h);
  }

  // 2. Reactivity. Feedback uses T from the previous step.
  pp_.Tcool = in.Tcool;
  const bool testMode = in.overrideOn && latched_ == 0;
  const float rho = testMode ? in.rhoOverride
                             : rodWorth(rod_, pp_) + safetyWorth(safety_, pp_) + feedback(dT_, pp_);

  // 3-5. Kinetics, heat balance, period meter.
  const bool ok = stepImplicit(k_, kp_, rho, pp_.S, h);
  dT_ = stepTemperature(dT_, k_.n, pp_, h);
  update(meter_, k_.n, h);

  tFrac_ += h;
  while (tFrac_ >= 1.0f) {
    tFrac_ -= 1.0f;
    ++tWhole_;
  }

  r_.t = static_cast<float>(tWhole_) + tFrac_;
  r_.n = k_.n;
  r_.rho = rho;
  r_.rhoDollars = rho / totalBeta(kp_);
  r_.T = pp_.Tref + dT_;
  r_.period = meter_.period;
  r_.sur = sur(meter_.period);
  r_.logP = logPower(k_.n);
  r_.rod = rod_;

  // 6. Trips latch.
  uint8_t present = evaluate(r_.n, r_.T, r_.period, r_.rhoDollars, sp_);
  if (!ok) present |= TRIP_FAULT;
  if (testMode) present &= static_cast<uint8_t>(~OVERRIDE_BYPASS);
  present_ = present;
  latched_ |= present;

  r_.trips = latched_;
  r_.warn = warnings(r_.n, r_.T, r_.period, sp_);
  return r_;
}
