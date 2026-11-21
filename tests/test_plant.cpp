// test_plant.cpp - rods, heat balance and feedback tests (T9-T12, T17, T18).
// Units: rod position 0..1, time s, rho dk/k, T K, P normalized power.
#include <math.h>

#include "doctest.h"
#include "kinetics.h"
#include "plant.h"

static const PlantParams PP = defaultPlant();

struct PlantRun {
  double n, T, rho, maxN, lateSpread;  // lateSpread: (max - min) n over the last third
  double tPeak, nAt540;
};

// Kinetics + rod + heat balance + feedback, no protection. Starts at full-power
// equilibrium with the rod at 0.5 and moves it toward rodTarget at normal speed.
static PlantRun runPlant(float rodTarget, float Tcool, double seconds) {
  const KineticsParams kp = keepinU235();
  PlantParams pp = PP;
  pp.Tcool = Tcool;
  const float h = 0.01f;
  Kinetics k;
  initEquilibrium(k, kp, 1.0f);
  float x = 0.5f, dT = 0.0f, rho = 0.0f;
  double maxN = k.n, lateMin = 1e30, lateMax = 0.0, tPeak = 0.0, nAt540 = 0.0;
  const int steps = static_cast<int>(seconds / h + 0.5);
  for (int i = 0; i < steps; ++i) {
    x = moveRod(x, rodTarget, pp.rodSpeed, h);
    rho = rodWorth(x, pp) + feedback(dT, pp);
    stepImplicit(k, kp, rho, pp.S, h);
    dT = stepTemperature(dT, k.n, pp, h);
    if (k.n > maxN) {
      maxN = k.n;
      tPeak = (i + 1) * h;
    }
    if (i + 1 == 54000) nAt540 = k.n;
    if (3 * i >= 2 * steps) {
      if (k.n < lateMin) lateMin = k.n;
      if (k.n > lateMax) lateMax = k.n;
    }
  }
  PlantRun r = {k.n, pp.Tref + dT, rho, maxN, lateMax - lateMin, tPeak, nAt540};
  return r;
}

// Rod position in [0.5, 1] where the regulating rod adds rho.
static float rodFor(float rho) {
  float lo = 0.5f, hi = 1.0f;
  for (int i = 0; i < 40; ++i) {
    const float mid = 0.5f * (lo + hi);
    (rodWorth(mid, PP) < rho ? lo : hi) = mid;
  }
  return 0.5f * (lo + hi);
}

TEST_CASE("T9 rod worth: 0, rho_w/2, rho_w at the ends and midpoint; critical at 0.5") {
  CHECK(PP.rhoW * sCurve(0.0f) == doctest::Approx(0.0));
  CHECK(PP.rhoW * sCurve(0.5f) == doctest::Approx(PP.rhoW / 2));
  CHECK(PP.rhoW * sCurve(1.0f) == doctest::Approx(PP.rhoW));
  CHECK(fabs(rodWorth(0.5f, PP)) < 1e-9);
  CHECK(rodWorth(0.0f, PP) / 0.0065 == doctest::Approx(-1.1));
  CHECK(safetyWorth(1.0f, PP) == doctest::Approx(0.0));
  CHECK((rodWorth(0.0f, PP) + safetyWorth(0.0f, PP)) / 0.0065 == doctest::Approx(-5.1));
}

TEST_CASE("T10 rod rate limit") {
  const float h = 0.01f, v = PP.rodSpeed;
  float x = 0.5f, maxStep = 0.0f, lo = 1.0f, hi = 0.0f;
  const float demands[] = {1.0f, 0.0f, 0.52f, 0.52f, 2.0f, -1.0f};
  for (float d : demands) {
    for (int i = 0; i < 100; ++i) {
      const float next = moveRod(x, d, v, h);
      maxStep = fmaxf(maxStep, fabsf(next - x));
      lo = fminf(lo, next);
      hi = fmaxf(hi, next);
      x = next;
    }
  }
  CHECK(maxStep <= v * h * 1.0001f);
  CHECK(lo >= 0.0f);
  CHECK(hi <= 1.0f);
  CHECK(moveRod(0.5f, 0.0f, PP.scramSpeed, 1.0f) == doctest::Approx(0.0));
}

TEST_CASE("TEST-04 moveRod exact steps, no overshoot, clamps") {
  CHECK(moveRod(0.5f, 1.0f, 0.02f, 0.01f) == doctest::Approx(0.5002).epsilon(1e-6));
  CHECK(moveRod(0.5f, 0.0f, 0.02f, 0.01f) == doctest::Approx(0.4998).epsilon(1e-6));
  CHECK(moveRod(0.5f, 0.50001f, 0.02f, 0.01f) == 0.50001f);
  CHECK(moveRod(0.9999f, 2.0f, 0.02f, 0.01f) == 1.0f);
  CHECK(moveRod(0.0001f, -1.0f, 0.02f, 0.01f) == 0.0f);
}

TEST_CASE("T11 thermal steady state T = Tc + P / K") {
  float dT = PP.Tcool - PP.Tref;
  for (int i = 0; i < 200000; ++i) dT = stepTemperature(dT, 1.5f, PP, 0.01f);  // 2000 s
  CHECK(PP.Tref + dT == doctest::Approx(PP.Tcool + 1.5 / PP.K).epsilon(1e-4));
  CHECK(feedback(0.0f, PP) == doctest::Approx(0.0));
  CHECK(isValid(PP));
  PlantParams bad = PP;
  bad.alphaT = 1.0e-5f;
  CHECK_FALSE(isValid(bad));
}

TEST_CASE("T12 +$0.2 rod pull settles where feedback cancels it") {
  const float rodRho = 0.2f * 0.0065f;
  const PlantRun r = runPlant(rodFor(rodRho), PP.Tcool, 1800.0);
  const double dT = rodRho / -PP.alphaT;  // 26 K
  CHECK(r.T - PP.Tref == doctest::Approx(dT).epsilon(0.02));
  CHECK(r.n == doctest::Approx(1.0 + dT * PP.K).epsilon(0.02));  // 1.87
  CHECK(fabs(r.rho) < 1.0e-5);
  // With a 150 s thermal lag the first overshoot is large (about 80 %, matches
  // tools/reference.py); the requirement is that the ringing dies out.
  const double overshoot = r.maxN / r.n - 1.0;
  INFO("T12 overshoot " << overshoot * 100.0 << " %, peak at " << r.tPeak << " s");
  CHECK(overshoot == doctest::Approx(0.81).epsilon(0.10));
  CHECK(r.tPeak == doctest::Approx(60.0).epsilon(0.25));
  CHECK(fabs(r.nAt540 / r.n - 1.0) < 0.02);
  CHECK(r.lateSpread / r.n < 0.01);
}

TEST_CASE("T17 rod S-curve vs DOE Example 3 (260 pcm over 16 in)") {
  const double doePcm[8] = {10, 30, 70, 130, 190, 230, 250, 260};  // at 2, 4, ... 16 in
  for (int i = 0; i < 8; ++i) {
    const float x = static_cast<float>(2 * (i + 1)) / 16.0f;
    CHECK(fabs(260.0 * sCurve(x) - doePcm[i]) <= 0.03 * 260.0);
  }
  CHECK(260.0 * sCurve(0.5f) == doctest::Approx(130.0));
}

TEST_CASE("T18 overcooling: Tc down 10 K, rod fixed, P to 1.33 and T back to Tref") {
  const PlantRun r = runPlant(0.5f, PP.Tcool - 10.0f, 1800.0);
  CHECK(fabs(r.T - PP.Tref) < 0.05);
  CHECK(fabs(r.rho) < 1.0e-5);
  CHECK(r.n == doctest::Approx(1.0 + 10.0 * PP.K).epsilon(0.02));
}
