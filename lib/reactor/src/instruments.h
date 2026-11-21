// instruments.h - period meter, startup rate and log power.
// Units: time s, period s, SUR decades per minute (DPM), n normalized power.
#ifndef REACTOR_INSTRUMENTS_H
#define REACTOR_INSTRUMENTS_H

#include <stdint.h>

const float PERIOD_INF = 1.0e9f;  // shown as infinity: power is steady
const int PERIOD_SAMPLES = 11;    // 11 samples 0.1 s apart = 1 s window

struct PeriodMeter {
  float lnN[PERIOD_SAMPLES];  // ring buffer of ln(n)
  uint8_t head;               // next slot to write, also the oldest sample once full
  uint8_t count;
  float acc;     // time since the last sample (s)
  float omega;   // EMA-smoothed inverse period (1/s)
  float period;  // 1 / omega, or PERIOD_INF
};

void initPeriodMeter(PeriodMeter& m);

// Windowed log-power slope over 1 s (F18). The inverse period is smoothed, not
// the period, so the meter passes cleanly through infinity when power turns.
void update(PeriodMeter& m, float n, float h);

float sur(float period);  // F19
float logPower(float n);  // F20

#endif
