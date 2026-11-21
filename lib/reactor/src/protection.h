// protection.h - trip and warning logic.
// Units: n normalized power, T K, period s, rho in dollars.
#ifndef REACTOR_PROTECTION_H
#define REACTOR_PROTECTION_H

#include <stdint.h>

#include "params.h"

const uint8_t TRIP_MANUAL = 1;
const uint8_t TRIP_HIGH_POWER = 2;
const uint8_t TRIP_HIGH_TEMP = 4;
const uint8_t TRIP_SHORT_PERIOD = 8;
const uint8_t TRIP_PROMPT_CRIT = 16;
const uint8_t TRIP_FAULT = 32;

const uint8_t WARN_POWER = 1;
const uint8_t WARN_TEMP = 2;
const uint8_t WARN_PERIOD = 4;

// Trip conditions present right now. The caller ORs them into a latched word.
uint8_t evaluate(float n, float T, float period, float rhoDollars, const Setpoints& s);

// Warning bits (not trips).
uint8_t warnings(float n, float T, float period, const Setpoints& s);

// Clears the latch only if the rods are in (rodPos < 0.01) and no trip condition is present.
bool tryReset(uint8_t& latched, float rodPos, uint8_t present);

#endif
