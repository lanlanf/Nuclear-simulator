// plant.h - control rods, lumped heat balance and temperature feedback.
// Units: rod position 0 = fully in, 1 = fully withdrawn; time s; rho dk/k; T K;
// P normalized power.
#ifndef REACTOR_PLANT_H
#define REACTOR_PLANT_H

#include "params.h"

// Integral worth fraction x - sin(2 pi x) / (2 pi): 0 at x = 0, 0.5 at x = 0.5, 1 at x = 1 (F14).
float sCurve(float x);

// Regulating rod reactivity, zero at x = 0.5 (F14).
float rodWorth(float x, const PlantParams& p);

// Safety bank reactivity, zero when fully withdrawn (s = 1), -rhoS when fully in.
float safetyWorth(float s, const PlantParams& p);

// Rate-limited move toward demand, clamped to [0, 1] (F15).
float moveRod(float x, float demand, float speed, float h);

// One backward Euler step of MC dT/dt = P - K (T - Tcool) (F16), written for
// dT = T - Tref. At T = 320 K a float step of 10 ms rounds away any power
// imbalance under 0.8 %; the deviation is small, so it keeps full resolution.
float stepTemperature(float dT, float P, const PlantParams& p, float h);

// alphaT (T - Tref) (F17), with dT = T - Tref.
float feedback(float dT, const PlantParams& p);

// Rejects a positive temperature coefficient and non-physical heat constants.
bool isValid(const PlantParams& p);

#endif
