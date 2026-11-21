// params.h - model constants and tunable parameters.
// Units: time s, power normalized (1 = 100 %), reactivity dk/k, temperature K.
// beta here is the physical U-235 delayed fraction (Keepin), not beta_eff.
#ifndef REACTOR_PARAMS_H
#define REACTOR_PARAMS_H

const int MAX_GROUPS = 6;
const float DOLLAR = 0.0065f;  // 1 $ of reactivity in dk/k

struct KineticsParams {
  int groups;
  float beta[MAX_GROUPS];    // delayed fraction per group (-)
  float lambda[MAX_GROUPS];  // precursor decay constant (1/s)
  float Lambda;              // prompt generation time (s)
};

// Keepin six-group thermal U-235 data.
inline KineticsParams keepinU235() {
  KineticsParams p = {6,
                      {0.000215f, 0.001424f, 0.001274f, 0.002568f, 0.000748f, 0.000273f},
                      {0.0124f, 0.0305f, 0.111f, 0.301f, 1.14f, 3.01f},
                      1.0e-4f};
  return p;
}

struct PlantParams {
  float rhoW;        // regulating rod total worth (dk/k)
  float rhoCrit;     // rod worth at the critical position, subtracted so x = 0.5 is critical
  float rhoS;        // safety bank total worth (dk/k)
  float rodSpeed;    // normal rod speed (fraction of stroke per s)
  float scramSpeed;  // scram rod speed (fraction of stroke per s)
  float MC;          // heat capacity (power * s / K)
  float K;           // heat removal coefficient (power / K)
  float Tcool;       // coolant temperature (K)
  float Tref;        // temperature where feedback is zero (K)
  float alphaT;      // temperature coefficient (dk/k per K), must be <= 0
  float S;           // external source (normalized power / s)
};

inline PlantParams defaultPlant() {
  PlantParams p;
  p.rhoW = 9.0f * DOLLAR;
  p.rhoCrit = 0.5f * p.rhoW;
  p.rhoS = 4.0f * DOLLAR;
  p.rodSpeed = 0.0049f;
  p.scramSpeed = 1.0f;
  p.MC = 0.25f;
  p.K = 1.0f / 30.0f;
  p.Tcool = 290.0f;
  p.Tref = 320.0f;  // Tcool + 1 / K: full power at the critical rod position
  p.alphaT = -7.5e-4f;
  p.S = 3.0e-4f;
  return p;
}

struct Setpoints {
  float powerTrip;   // normalized power
  float tempTrip;    // K
  float periodTrip;  // s, trips when 0 < period < this
  float powerWarn;
  float tempWarn;
  float periodWarn;
};

inline Setpoints defaultSetpoints() {
  Setpoints s = {1.20f, 330.0f, 10.0f, 1.05f, 325.0f, 30.0f};
  return s;
}

#endif
