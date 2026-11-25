// sim_cli.cpp - desktop runner that writes CSV to stdout.
// Units: t s, rho $, n normalized power, T K, period s, sur DPM, rod 0..1.
// hold/step/ramp: kinetics only (validation against tools/reference.py).
// rodpull: kinetics + rod + heat balance + feedback, no protection (V8).
// scram/startup: the full Reactor facade with protection.
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "instruments.h"
#include "kinetics.h"
#include "plant.h"
#include "reactor.h"

static void usage() {
  fprintf(stderr,
          "usage: sim_cli --scenario {hold,step,ramp,scram,rodpull,startup}\n"
          "               [--rho-dollars X] [--dt S] [--method implicit|explicit] [--t-end S]\n");
}

static void row(double t, float rhoDollars, float n, float T, float period, float rod, int trip) {
  printf("%.4f,%.6g,%.8g,%.6g,%.6g,%.6g,%.6g,%d", t, static_cast<double>(rhoDollars),
         static_cast<double>(n), static_cast<double>(T), static_cast<double>(period),
         static_cast<double>(sur(period)), static_cast<double>(rod), trip);
}

// Regulating rod position in [0.5, 1] that adds rho. note: same bisection as tests/test_plant.cpp.
static float rodFor(float rho, const PlantParams& pp) {
  float lo = 0.5f, hi = 1.0f;
  for (int i = 0; i < 40; ++i) {
    const float mid = 0.5f * (lo + hi);
    (rodWorth(mid, pp) < rho ? lo : hi) = mid;
  }
  return 0.5f * (lo + hi);
}

int main(int argc, char** argv) {
  const char* scenario = nullptr;
  const char* method = "implicit";
  float rhoDollars = NAN;
  float dt = 0.01f;
  double tEnd = NAN;
  for (int i = 1; i + 1 < argc; i += 2) {
    if (!strcmp(argv[i], "--scenario")) scenario = argv[i + 1];
    else if (!strcmp(argv[i], "--rho-dollars")) rhoDollars = strtof(argv[i + 1], nullptr);
    else if (!strcmp(argv[i], "--dt")) dt = strtof(argv[i + 1], nullptr);
    else if (!strcmp(argv[i], "--method")) method = argv[i + 1];
    else if (!strcmp(argv[i], "--t-end")) tEnd = strtod(argv[i + 1], nullptr);
    else {
      usage();
      return 2;
    }
  }
  if (!scenario || !(dt > 0.0f)) {
    usage();
    return 2;
  }
  const bool explicitEuler = !strcmp(method, "explicit");
  const bool kineticsOnly =
      !strcmp(scenario, "hold") || !strcmp(scenario, "step") || !strcmp(scenario, "ramp");
  if (explicitEuler && !kineticsOnly) {
    fprintf(stderr, "--method explicit only works with hold, step and ramp\n");
    return 2;
  }
  if (isnan(tEnd)) tEnd = !strcmp(scenario, "startup") ? 4000.0 : 300.0;
  if (isnan(rhoDollars)) rhoDollars = !strcmp(scenario, "ramp") ? 0.5f : !strcmp(scenario, "rodpull") ? 0.2f : 0.1f;

  const KineticsParams kp = keepinU235();
  const float beta = totalBeta(kp);
  const long steps = static_cast<long>(tEnd / static_cast<double>(dt) + 0.5);
  long every = lround(0.1 / static_cast<double>(dt));
  if (every < 1) every = 1;
  printf("t,rho,n,T,period,sur,rod,trip%s\n", !strcmp(scenario, "startup") ? ",inv_m" : "");

  if (kineticsOnly) {
    const PlantParams pp = defaultPlant();
    const float target = !strcmp(scenario, "hold") ? 0.0f : rhoDollars * beta;
    Kinetics k;
    initEquilibrium(k, kp, 1.0f);
    PeriodMeter m;
    initPeriodMeter(m);
    row(0.0, 0.0f, k.n, pp.Tref, m.period, 0.5f, 0);
    printf("\n");
    for (long i = 1; i <= steps; ++i) {
      const double t = static_cast<double>(i) * static_cast<double>(dt);
      float rho = target;
      if (!strcmp(scenario, "ramp") && t < 10.0) rho = target * static_cast<float>(t / 10.0);
      if (explicitEuler) stepExplicit(k, kp, rho, 0.0f, dt);
      else stepImplicit(k, kp, rho, 0.0f, dt);
      update(m, k.n, dt);
      if (i % every == 0) {
        row(t, rho / beta, k.n, pp.Tref, m.period, 0.5f, 0);
        printf("\n");
      }
    }
    return 0;
  }

  if (!strcmp(scenario, "rodpull")) {
    const PlantParams pp = defaultPlant();
    const float xTarget = rodFor(rhoDollars * beta, pp);
    Kinetics k;
    initEquilibrium(k, kp, 1.0f);
    PeriodMeter m;
    initPeriodMeter(m);
    float x = 0.5f, dT = 0.0f;
    row(0.0, 0.0f, k.n, pp.Tref, m.period, x, 0);
    printf("\n");
    for (long i = 1; i <= steps; ++i) {
      x = moveRod(x, xTarget, pp.rodSpeed, dt);
      const float rho = rodWorth(x, pp) + feedback(dT, pp);
      stepImplicit(k, kp, rho, pp.S, dt);
      dT = stepTemperature(dT, k.n, pp, dt);
      update(m, k.n, dt);
      if (i % every == 0) {
        row(static_cast<double>(i) * static_cast<double>(dt), rho / beta, k.n, pp.Tref + dT, m.period, x, 0);
        printf("\n");
      }
    }
    return 0;
  }

  const bool startup = !strcmp(scenario, "startup");
  if (!startup && strcmp(scenario, "scram") != 0) {
    usage();
    return 2;
  }
  Reactor reactor;
  reactor.init();
  Inputs in;
  float nRef = 1.0f;
  if (startup) {
    // Silent lead-in: trip, cool down to source level, reset, withdraw the safety bank.
    in.scram = true;
    reactor.step(in, dt);
    in.scram = false;
    in.rodDemand = 0.0f;
    for (long i = 0; i < static_cast<long>(3000.0f / dt); ++i) reactor.step(in, dt);
    in.reset = true;
    reactor.step(in, dt);
    in.reset = false;
    const Readings* r = nullptr;
    for (long i = 0; i < static_cast<long>(300.0f / dt); ++i) r = &reactor.step(in, dt);
    if (r->trips != 0) {
      fprintf(stderr, "startup lead-in tripped (0x%02x)\n", r->trips);
      return 1;
    }
    nRef = r->n;
    row(0.0, r->rhoDollars, r->n, r->T, r->period, r->rod, r->trips);
    printf(",1\n");
    in.rodDemand = 0.5f;  // one knob turn; the rod stop paces the approach
  } else {
    const Readings& r = reactor.step(in, 0.0f);
    row(0.0, r.rhoDollars, r.n, r.T, r.period, r.rod, r.trips);
    printf("\n");
  }
  for (long i = 1; i <= steps; ++i) {
    const double t = static_cast<double>(i) * static_cast<double>(dt);
    if (!startup) in.scram = t >= 1.0 && t < 1.0 + static_cast<double>(dt);
    const Readings& r = reactor.step(in, dt);
    if (i % every == 0) {
      row(t, r.rhoDollars, r.n, r.T, r.period, r.rod, r.trips);
      if (startup) printf(",%.6g\n", static_cast<double>(nRef / r.n));
      else printf("\n");
    }
  }
  return 0;
}
