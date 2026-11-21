// test_reactor.cpp - period meter, protection and the Reactor facade (T13-T15).
// Units: time s, n normalized power, rho dk/k, T K, period s.
#include <math.h>

#include "doctest.h"
#include "instruments.h"
#include "kinetics.h"
#include "plant.h"
#include "protection.h"
#include "reactor.h"

static Readings runFor(Reactor& r, const Inputs& in, double seconds) {
  REQUIRE(seconds >= 0.01);
  Readings out = {};
  for (int i = 0; i < static_cast<int>(seconds / 0.01 + 0.5); ++i) out = r.step(in, 0.01f);
  return out;
}

// Rod position in [0.5, 1] adding rho. note: duplicated in test_plant.cpp (two uses).
static float rodFor(float rho) {
  const PlantParams pp = defaultPlant();
  float lo = 0.5f, hi = 1.0f;
  for (int i = 0; i < 40; ++i) {
    const float mid = 0.5f * (lo + hi);
    (rodWorth(mid, pp) < rho ? lo : hi) = mid;
  }
  return 0.5f * (lo + hi);
}

static const Setpoints RELAXED = {10.0f, 1.0e9f, 0.0f, 10.0f, 1.0e9f, 0.0f};

TEST_CASE("TEST-01 facade +$0.2 rod pull settles with feedback") {
  Reactor r;
  REQUIRE(r.init(defaultPlant(), RELAXED));
  Inputs in;
  in.rodDemand = rodFor(0.2f * 0.0065f);
  const Readings out = runFor(r, in, 1800.0);
  CHECK(out.trips == 0);
  CHECK(out.T - 320.0 == doctest::Approx(26.0).epsilon(0.03));
  CHECK(out.n == doctest::Approx(1.867).epsilon(0.02));
  CHECK(fabs(out.rho) < 1.0e-5);
}

TEST_CASE("TEST-01 facade Tcool 280 overcooling") {
  Reactor r;
  REQUIRE(r.init(defaultPlant(), RELAXED));
  Inputs in;
  in.Tcool = 280.0f;
  const Readings out = runFor(r, in, 1800.0);
  CHECK(out.trips == 0);
  CHECK(out.n == doctest::Approx(1.333).epsilon(0.02));
  CHECK(fabs(out.T - 320.0) < 0.05);
}

TEST_CASE("TEST-02 solver failure latches TRIP_FAULT even in override mode") {
  Reactor r;
  REQUIRE(r.init());
  Inputs in;
  in.overrideOn = true;
  in.rhoOverride = 20.0f * totalBeta(keepinU235());
  const Readings out = r.step(in, 0.01f);
  CHECK((out.trips & TRIP_FAULT) != 0);
  CHECK(isfinite(out.n));
  CHECK((evaluate(NAN, 320.0f, PERIOD_INF, 0.0f, defaultSetpoints()) & TRIP_FAULT) != 0);
  CHECK((evaluate(1.0f, NAN, PERIOD_INF, 0.0f, defaultSetpoints()) & TRIP_FAULT) != 0);
}

TEST_CASE("TEST-03 setpoint values are pinned") {
  const Setpoints s = defaultSetpoints();
  CHECK(s.powerTrip == 1.20f);
  CHECK(s.tempTrip == 330.0f);
  CHECK(s.periodTrip == 10.0f);
  CHECK(s.powerWarn == 1.05f);
  CHECK(s.tempWarn == 325.0f);
  CHECK(s.periodWarn == 30.0f);
}

TEST_CASE("TEST-03 trip edges") {
  const Setpoints s = defaultSetpoints();
  const float I = PERIOD_INF;
  CHECK(evaluate(1.0f, 320.0f, I, 0.0f, s) == 0);
  CHECK(evaluate(1.20f, 320.0f, I, 0.0f, s) == 0);
  CHECK(evaluate(1.2001f, 320.0f, I, 0.0f, s) == TRIP_HIGH_POWER);
  CHECK(evaluate(1.0f, 330.0f, I, 0.0f, s) == 0);
  CHECK(evaluate(1.0f, 330.01f, I, 0.0f, s) == TRIP_HIGH_TEMP);
  CHECK(evaluate(1.0f, 320.0f, 10.0f, 0.0f, s) == 0);
  CHECK(evaluate(1.0f, 320.0f, 9.99f, 0.0f, s) == TRIP_SHORT_PERIOD);
  CHECK(evaluate(1.0f, 320.0f, -5.0f, 0.0f, s) == 0);
  CHECK(evaluate(1.0f, 320.0f, I, 0.999f, s) == 0);
  CHECK(evaluate(1.0f, 320.0f, I, 1.0f, s) == TRIP_PROMPT_CRIT);
  CHECK(totalBeta(keepinU235()) == doctest::Approx(0.0065).epsilon(1e-4));
}

TEST_CASE("TEST-03 warning edges") {
  const Setpoints s = defaultSetpoints();
  const float I = PERIOD_INF;
  CHECK(warnings(1.05f, 320.0f, I, s) == 0);
  CHECK(warnings(1.0501f, 320.0f, I, s) == WARN_POWER);
  CHECK(warnings(1.0f, 325.0f, I, s) == 0);
  CHECK(warnings(1.0f, 325.01f, I, s) == WARN_TEMP);
  CHECK(warnings(1.0f, 320.0f, 30.0f, s) == 0);
  CHECK(warnings(1.0f, 320.0f, 29.99f, s) == WARN_PERIOD);
  CHECK(warnings(1.0f, 320.0f, -5.0f, s) == 0);
}

TEST_CASE("TEST-03 facade high coolant temperature trips HIGH_TEMP") {
  Reactor r;
  REQUIRE(r.init());
  Inputs in;
  in.Tcool = 340.0f;  // 325 K never reaches 330: feedback cuts power first
  const Readings out = runFor(r, in, 300.0);
  CHECK((out.trips & TRIP_HIGH_TEMP) != 0);
  CHECK((out.trips & TRIP_FAULT) == 0);
}

TEST_CASE("T15 period meter recovers a known period") {
  const float taus[] = {20.0f, -50.0f};
  for (float tau : taus) {
    PeriodMeter m;
    initPeriodMeter(m);
    for (int i = 1; i <= 3000; ++i) update(m, expf(0.01f * static_cast<float>(i) / tau), 0.01f);
    CHECK(m.period == doctest::Approx(tau).epsilon(0.01));
  }
  PeriodMeter steady;
  initPeriodMeter(steady);
  for (int i = 0; i < 3000; ++i) update(steady, 1.0f, 0.01f);
  CHECK(steady.period == PERIOD_INF);
  CHECK(sur(26.06f) == doctest::Approx(1.0));
  CHECK(logPower(0.01f) == doctest::Approx(-2.0));
}

TEST_CASE("Reactor holds full power at the critical rod position") {
  Reactor r;
  REQUIRE(r.init());
  const Inputs in;
  const Readings& out = runFor(r, in, 600.0);
  CHECK(fabs(out.n - 1.0) < 1e-3);
  CHECK(out.T == doctest::Approx(320.0).epsilon(1e-4));
  CHECK(out.trips == 0);
  CHECK(out.warn == 0);
  CHECK(out.t == doctest::Approx(600.0).epsilon(1e-5));
}

TEST_CASE("Reactor rejects a positive temperature coefficient") {
  PlantParams bad = defaultPlant();
  bad.alphaT = 1.0e-5f;
  Reactor r;
  CHECK_FALSE(r.init(bad));
}

TEST_CASE("T13 trip latches; reset only with rods and rod demand in") {
  Reactor r;
  REQUIRE(r.init());
  Inputs in;
  runFor(r, in, 1.0);

  in.scram = true;
  CHECK((r.step(in, 0.01f).trips & TRIP_MANUAL) != 0);
  in.scram = false;
  CHECK(runFor(r, in, 0.3).trips == TRIP_MANUAL);  // latch holds after release

  in.rodDemand = 0.0f;
  in.reset = true;
  CHECK(r.step(in, 0.01f).trips != 0);  // rod still moving in
  in.reset = false;
  const Readings& in1 = runFor(r, in, 2.0);
  CHECK(in1.rod < 0.01f);

  in.rodDemand = 0.5f;
  in.reset = true;
  CHECK(r.step(in, 0.01f).trips != 0);  // rod pot still out
  in.rodDemand = 0.0f;
  CHECK(r.step(in, 0.01f).trips == 0);

  in.reset = false;
  const Readings& after = runFor(r, in, 100.0);  // safety bank withdraws, rod stays in
  CHECK(after.trips == 0);
  CHECK(after.rho < 0.0f);
  CHECK(after.n < 0.1f);
}

TEST_CASE("SAFE-02 scram and reset in the same step stays tripped") {
  Reactor r;
  REQUIRE(r.init());
  Inputs in;
  runFor(r, in, 1.0);
  in.scram = true;
  in.rodDemand = 0.0f;
  runFor(r, in, 2.0);  // rods fully in

  in.reset = true;
  CHECK((r.step(in, 0.01f).trips & TRIP_MANUAL) != 0);
  CHECK((r.step(in, 0.01f).trips & TRIP_MANUAL) != 0);
  in.scram = false;
  CHECK(r.step(in, 0.01f).trips == 0);  // plain reset still works
}

TEST_CASE("SAFE-04 non-finite rod demand fault-trips, rod stays finite") {
  const float bad[] = {NAN, INFINITY, -INFINITY};
  for (float v : bad) {
    Reactor r;
    REQUIRE(r.init());
    Inputs in;
    in.rodDemand = v;
    const Readings& one = r.step(in, 0.01f);
    CHECK((one.trips & TRIP_FAULT) != 0);
    CHECK(isfinite(one.rod));
    CHECK(isfinite(one.n));
    const Readings& later = runFor(r, in, 2.0);
    CHECK(later.rod == 0.0f);
    CHECK(isfinite(later.n));

    in.reset = true;  // bad input persists: reset refused
    CHECK((r.step(in, 0.01f).trips & TRIP_FAULT) != 0);
    in.rodDemand = 0.0f;
    r.step(in, 0.01f);  // input good again; present fault clears
    CHECK(r.step(in, 0.01f).trips == 0);
  }
}

TEST_CASE("SAFE-05 non-finite coolant temperature fault-trips, T stays finite") {
  const float bad[] = {NAN, INFINITY, -INFINITY};
  for (float v : bad) {
    Reactor r;
    REQUIRE(r.init());
    Inputs in;
    in.Tcool = v;
    const Readings& one = r.step(in, 0.01f);
    CHECK((one.trips & TRIP_FAULT) != 0);
    CHECK(isfinite(one.T));
    const Readings& later = runFor(r, in, 2.0);
    CHECK(isfinite(later.T));
    CHECK(isfinite(later.n));
  }
  Reactor r;
  REQUIRE(r.init());
  Inputs in;
  in.overrideOn = true;
  in.Tcool = NAN;
  CHECK((r.step(in, 0.01f).trips & TRIP_FAULT) != 0);  // FAULT is not bypassed
}

TEST_CASE("T14 prompt critical trips with finite readings, then shuts down") {
  Reactor r;
  REQUIRE(r.init());
  Inputs in;
  in.overrideOn = true;
  in.rhoOverride = 1.2f * 0.0065f;
  const Readings& hit = runFor(r, in, 0.1);
  CHECK((hit.trips & TRIP_PROMPT_CRIT) != 0);
  CHECK(isfinite(hit.n));
  CHECK(isfinite(hit.T));
  CHECK((hit.trips & TRIP_FAULT) == 0);
  const Readings& later = runFor(r, in, 600.0);  // override is ignored while tripped
  CHECK(later.n < 0.01f);
  CHECK(isfinite(later.n));
}

TEST_CASE("Override is a test mode: power and period trips are bypassed") {
  Reactor r;
  REQUIRE(r.init());
  Inputs in;
  in.overrideOn = true;
  in.rhoOverride = 0.5f * 0.0065f;
  const Readings& out = runFor(r, in, 20.0);
  CHECK(out.trips == 0);
  CHECK(out.n > 2.0f);
  CHECK(out.period < 10.0f);
  CHECK((out.warn & WARN_POWER) != 0);

  in.overrideOn = false;
  CHECK((r.step(in, 0.01f).trips & TRIP_HIGH_POWER) != 0);
}

TEST_CASE("Fast rod withdrawal trips on high power, not on a fault") {
  Reactor r;
  REQUIRE(r.init());
  Inputs in;
  in.rodDemand = 1.0f;
  const Readings& out = runFor(r, in, 30.0);
  CHECK(out.trips != 0);
  CHECK((out.trips & (TRIP_HIGH_POWER | TRIP_SHORT_PERIOD)) != 0);
  CHECK((out.trips & TRIP_FAULT) == 0);
}
