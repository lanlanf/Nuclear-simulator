// kinetics.cpp - six-group point kinetics solvers.
// Units: time s, n normalized power, rho dk/k, S 1/s.
#include "kinetics.h"

#include <math.h>

float totalBeta(const KineticsParams& p) {
  float beta = 0.0f;
  for (int i = 0; i < p.groups; ++i) beta += p.beta[i];
  return beta;
}

void initEquilibrium(Kinetics& k, const KineticsParams& p, float n0) {
  k.n = n0;
  for (int i = 0; i < MAX_GROUPS; ++i) {
    k.C[i] = i < p.groups ? p.beta[i] * n0 / (p.lambda[i] * p.Lambda) : 0.0f;
  }
}

bool stepImplicit(Kinetics& k, const KineticsParams& p, float rho, float S, float h) {
  const float beta = totalBeta(p);
  const int substeps = rho >= beta ? 10 : 1;
  const float hs = h / static_cast<float>(substeps);

  for (int s = 0; s < substeps; ++s) {
    float num = k.n + hs * S;
    float sumLB = 0.0f;
    for (int i = 0; i < p.groups; ++i) {
      const float d = 1.0f + hs * p.lambda[i];
      num += hs * p.lambda[i] * k.C[i] / d;
      sumLB += p.lambda[i] * p.beta[i] / d;
    }
    const float den = 1.0f - hs * (rho - beta) / p.Lambda - hs * hs / p.Lambda * sumLB;
    if (!(den > 0.0f)) return false;

    float n = num / den;
    if (n > N_MAX) n = N_MAX;
    for (int i = 0; i < p.groups; ++i) {
      k.C[i] = (k.C[i] + hs * p.beta[i] * n / p.Lambda) / (1.0f + hs * p.lambda[i]);
    }
    k.n = n;
  }
  return isfinite(k.n);
}

void stepExplicit(Kinetics& k, const KineticsParams& p, float rho, float S, float h) {
  float dn = (rho - totalBeta(p)) / p.Lambda * k.n + S;
  for (int i = 0; i < p.groups; ++i) {
    dn += p.lambda[i] * k.C[i];
    k.C[i] += h * (p.beta[i] * k.n / p.Lambda - p.lambda[i] * k.C[i]);
  }
  k.n += h * dn;
}
