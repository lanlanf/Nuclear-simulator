// test_kinetics.cpp - point kinetics solver tests (T1-T8, T14 solver part, T16).
// Units: time s, n normalized power, rho dk/k.
#include <math.h>

#include "doctest.h"
#include "kinetics.h"

static const KineticsParams P = keepinU235();
static const double BETA = 0.0065;

static void run(Kinetics& k, float rho, float S, float h, int steps) {
  for (int i = 0; i < steps; ++i) stepImplicit(k, P, rho, S, h);
}

// Stable inverse period omega (1/s) from the six-group inhour equation (F9):
// rho = omega Lambda + sum beta_i omega / (omega + lambda_i). Bisection in double.
static double inhourOmega(double rho) {
  double lo = rho > 0 ? 0.0 : -P.lambda[0] + 1e-12;
  double hi = rho > 0 ? 1.0e3 : 0.0;
  for (int it = 0; it < 200; ++it) {
    const double w = 0.5 * (lo + hi);
    double f = w * P.Lambda - rho;
    for (int i = 0; i < P.groups; ++i) f += P.beta[i] * w / (w + P.lambda[i]);
    (f > 0 ? hi : lo) = w;
  }
  return 0.5 * (lo + hi);
}

// Asymptotic period measured from ln n between t1 and t2 of a step from critical.
static double measuredPeriod(float rho, float h, double t1, double t2) {
  Kinetics k;
  initEquilibrium(k, P, 1.0f);
  run(k, rho, 0.0f, h, static_cast<int>(t1 / h + 0.5));
  const double n1 = k.n;
  run(k, rho, 0.0f, h, static_cast<int>((t2 - t1) / h + 0.5));
  return (t2 - t1) / log(k.n / n1);
}

TEST_CASE("T1 critical reactor holds power for 60000 steps") {
  Kinetics k;
  initEquilibrium(k, P, 1.0f);
  run(k, 0.0f, 0.0f, 0.01f, 60000);
  CHECK(fabs(k.n - 1.0) < 1e-4);
}

TEST_CASE("T2 prompt jump for a +$0.31 step") {
  Kinetics k;
  initEquilibrium(k, P, 1.0f);
  run(k, 0.31f * 0.0065f, 0.0f, 0.001f, 100);  // 0.1 s
  // F11: beta / (beta - rho) = 1.449, plus a little delayed growth.
  CHECK(k.n == doctest::Approx(1.45).epsilon(0.02));
}

TEST_CASE("T3 +100 pcm period matches the inhour root") {
  const double tau = 1.0 / inhourOmega(0.001);
  CHECK(tau > 55.0);
  CHECK(tau < 57.0);
  CHECK(measuredPeriod(0.001f, 0.01f, 300.0, 500.0) == doctest::Approx(tau).epsilon(0.02));
}

TEST_CASE("T4 +$0.1 and +$0.5 periods match the inhour root") {
  const double tau01 = 1.0 / inhourOmega(0.1 * BETA);
  const double tau05 = 1.0 / inhourOmega(0.5 * BETA);
  CHECK(tau01 == doctest::Approx(98.0).epsilon(0.02));
  CHECK(tau05 == doctest::Approx(5.8).epsilon(0.03));
  CHECK(measuredPeriod(0.1f * 0.0065f, 0.01f, 400.0, 600.0) == doctest::Approx(tau01).epsilon(0.02));
  CHECK(measuredPeriod(0.5f * 0.0065f, 0.01f, 40.0, 60.0) == doctest::Approx(tau05).epsilon(0.02));
}

TEST_CASE("T5 SCRAM to -$5: prompt drop to 1/6, then the -80 s period") {
  const float rho = -5.0f * 0.0065f;
  Kinetics k;
  initEquilibrium(k, P, 1.0f);
  run(k, rho, 0.0f, 0.001f, 50);  // 0.05 s: prompt drop done, delayed decay barely started
  CHECK(k.n == doctest::Approx(1.0 / 6.0).epsilon(0.05));

  const double tau = 1.0 / inhourOmega(rho);
  CHECK(tau == doctest::Approx(-80.6).epsilon(0.05));
  CHECK(measuredPeriod(rho, 0.01f, 400.0, 600.0) == doctest::Approx(tau).epsilon(0.02));
}

TEST_CASE("T6 explicit Euler diverges above its stability limit, implicit does not") {
  // Limit h < 2 Lambda / (beta - rho) = 34 ms at +$0.1 (31 ms at critical).
  const float rho = 0.1f * 0.0065f;
  Kinetics ex, im;
  initEquilibrium(ex, P, 1.0f);
  initEquilibrium(im, P, 1.0f);
  bool diverged = false;
  for (int i = 0; i < 250 && !diverged; ++i) {  // 10 s at h = 40 ms
    stepExplicit(ex, P, rho, 0.0f, 0.04f);
    stepImplicit(im, P, rho, 0.0f, 0.04f);
    diverged = !(fabs(ex.n) < 1e6) || ex.n < 0.0f;
  }
  CHECK(diverged);
  CHECK(im.n > 1.0f);
  CHECK(im.n < 2.0f);

  Kinetics ok;  // below the limit explicit Euler is fine
  initEquilibrium(ok, P, 1.0f);
  for (int i = 0; i < 500; ++i) stepExplicit(ok, P, rho, 0.0f, 0.02f);
  CHECK(ok.n == doctest::Approx(1.2).epsilon(0.1));
}

TEST_CASE("T7 halving h halves the error (first order)") {
  const float rho = 0.5f * 0.0065f;
  const double tEnd = 5.0;
  double nAt[4];
  const float hs[4] = {0.04f, 0.02f, 0.01f, 0.0005f};
  for (int j = 0; j < 4; ++j) {
    Kinetics k;
    initEquilibrium(k, P, 1.0f);
    run(k, rho, 0.0f, hs[j], static_cast<int>(tEnd / hs[j] + 0.5));
    nAt[j] = k.n;
  }
  const double e1 = fabs(nAt[0] - nAt[3]);
  const double e2 = fabs(nAt[1] - nAt[3]);
  const double e3 = fabs(nAt[2] - nAt[3]);
  CHECK(e1 / e2 == doctest::Approx(2.0).epsilon(0.2));
  CHECK(e2 / e3 == doctest::Approx(2.0).epsilon(0.2));
}

TEST_CASE("T8 subcritical with a source settles at -S Lambda / rho") {
  const float rho = -1.0f * 0.0065f;
  const float S = 3.0e-4f;
  Kinetics k;
  initEquilibrium(k, P, 1.0e-3f);
  run(k, rho, S, 0.1f, 30000);  // 3000 s
  CHECK(k.n == doctest::Approx(-S * P.Lambda / rho).epsilon(0.01));
}

TEST_CASE("T14 prompt critical: substeps keep the denominator positive and n finite") {
  Kinetics k;
  initEquilibrium(k, P, 1.0f);
  bool ok = true;
  for (int i = 0; i < 100; ++i) ok = ok && stepImplicit(k, P, 1.5f * 0.0065f, 0.0f, 0.01f);
  CHECK(ok);
  CHECK(isfinite(k.n));
  CHECK(k.n <= N_MAX);
  CHECK(k.n == doctest::Approx(N_MAX));
}

TEST_CASE("T16 halving a subcritical rho doubles the source-driven level") {
  const float S = 3.0e-4f;
  const float rho2 = -2.0f * 0.0065f;
  Kinetics k;
  initEquilibrium(k, P, -S * P.Lambda / rho2);
  run(k, rho2, S, 0.1f, 20000);  // 2000 s: stays put
  const double n2 = k.n;
  CHECK(n2 == doctest::Approx(-S * P.Lambda / rho2).epsilon(1e-3));
  run(k, 0.5f * rho2, S, 0.1f, 30000);  // 3000 s at -$1
  CHECK(k.n / n2 == doctest::Approx(2.0).epsilon(0.01));
}
