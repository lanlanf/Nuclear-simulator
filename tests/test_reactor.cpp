// test_reactor.cpp - period meter, protection and the Reactor facade (T13-T15).
// Units: time s, n normalized power, rho dk/k, T K, period s.
#include <math.h>

#include "doctest.h"
#include "instruments.h"
#include "protection.h"
#include "reactor.h"

static const Readings& runFor(Reactor& r, const Inputs& in, double seconds) {
  const Readings* out = nullptr;
  for (int i = 0; i < static_cast<int>(seconds / 0.01 + 0.5); ++i) out = &r.step(in, 0.01f);
  return *out;
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
