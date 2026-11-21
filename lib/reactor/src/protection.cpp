// protection.cpp - trip and warning logic.
// Units: n normalized power, T K, period s, rho in dollars.
#include "protection.h"

#include <math.h>

uint8_t evaluate(float n, float T, float period, float rhoDollars, const Setpoints& s) {
  uint8_t trips = 0;
  if (!isfinite(n) || !isfinite(T)) trips |= TRIP_FAULT;
  if (n > s.powerTrip) trips |= TRIP_HIGH_POWER;
  if (T > s.tempTrip) trips |= TRIP_HIGH_TEMP;
  if (period > 0.0f && period < s.periodTrip) trips |= TRIP_SHORT_PERIOD;
  if (rhoDollars >= 1.0f) trips |= TRIP_PROMPT_CRIT;
  return trips;
}

uint8_t warnings(float n, float T, float period, const Setpoints& s) {
  uint8_t warn = 0;
  if (n > s.powerWarn) warn |= WARN_POWER;
  if (T > s.tempWarn) warn |= WARN_TEMP;
  if (period > 0.0f && period < s.periodWarn) warn |= WARN_PERIOD;
  return warn;
}

bool tryReset(uint8_t& latched, float rodPos, uint8_t present) {
  if (rodPos >= 0.01f || present != 0) return false;
  latched = 0;
  return true;
}
