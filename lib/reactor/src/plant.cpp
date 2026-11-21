// plant.cpp - control rods, lumped heat balance and temperature feedback.
// Units: rod position 0..1 (1 = withdrawn), time s, rho dk/k, T K, P normalized.
#include "plant.h"

#include <math.h>

static const float TAU = 6.2831853f;  // 2 pi (Arduino.h already defines TWO_PI)

float sCurve(float x) { return x - sinf(TAU * x) / TAU; }

float rodWorth(float x, const PlantParams& p) { return p.rhoW * sCurve(x) - p.rhoCrit; }

float safetyWorth(float s, const PlantParams& p) { return p.rhoS * (sCurve(s) - 1.0f); }

float moveRod(float x, float demand, float speed, float h) {
  if (demand < 0.0f) demand = 0.0f;
  if (demand > 1.0f) demand = 1.0f;
  const float maxStep = speed * h;
  float dx = demand - x;
  if (dx > maxStep) dx = maxStep;
  if (dx < -maxStep) dx = -maxStep;
  return x + dx;
}

float stepTemperature(float dT, float P, const PlantParams& p, float h) {
  return (dT + (h / p.MC) * (P - p.K * (p.Tref - p.Tcool))) / (1.0f + h * p.K / p.MC);
}

float feedback(float dT, const PlantParams& p) { return p.alphaT * dT; }

bool isValid(const PlantParams& p) { return p.alphaT <= 0.0f && p.MC > 0.0f && p.K > 0.0f; }
