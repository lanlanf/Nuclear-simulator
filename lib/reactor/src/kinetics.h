// kinetics.h - six-group point kinetics with an external source.
// Units: time s, n normalized power, C_i in the same units as n, rho dk/k, S 1/s.
#ifndef REACTOR_KINETICS_H
#define REACTOR_KINETICS_H

#include "params.h"

const float N_MAX = 1.0e6f;  // power clamp, keeps float finite above prompt critical

struct Kinetics {
  float n;
  float C[MAX_GROUPS];
};

float totalBeta(const KineticsParams& p);

// Precursors in equilibrium with power n0 (F2).
void initEquilibrium(Kinetics& k, const KineticsParams& p, float n0);

// Backward Euler, closed form (F7). Uses 10 substeps when rho >= beta.
// Returns false if the denominator is not positive or n is not finite.
bool stepImplicit(Kinetics& k, const KineticsParams& p, float rho, float S, float h);

// Explicit Euler (F8). Only for the instability demo and tests.
void stepExplicit(Kinetics& k, const KineticsParams& p, float rho, float S, float h);

#endif
