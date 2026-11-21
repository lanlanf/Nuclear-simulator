// instruments.cpp - period meter, startup rate and log power.
// Units: time s, period s, SUR DPM, n normalized power.
#include "instruments.h"

#include <math.h>

static const float SAMPLE_DT = 0.1f;  // s between ln(n) samples
static const float WINDOW = SAMPLE_DT * static_cast<float>(PERIOD_SAMPLES - 1);
static const float EMA = 0.2f;         // smoothing weight per sample
static const float OMEGA_MIN = 1.0e-4f;  // below this |omega| the period reads infinity
static const float N_MIN = 1.0e-30f;

void initPeriodMeter(PeriodMeter& m) {
  for (int i = 0; i < PERIOD_SAMPLES; ++i) m.lnN[i] = 0.0f;
  m.head = 0;
  m.count = 0;
  m.acc = 0.0f;
  m.omega = 0.0f;
  m.period = PERIOD_INF;
}

void update(PeriodMeter& m, float n, float h) {
  m.acc += h;
  if (m.acc + 0.5f * h < SAMPLE_DT) return;
  m.acc -= SAMPLE_DT;

  const float ln = logf(n > N_MIN ? n : N_MIN);
  m.lnN[m.head] = ln;
  m.head = static_cast<uint8_t>((m.head + 1) % PERIOD_SAMPLES);
  if (m.count < PERIOD_SAMPLES) {
    ++m.count;
    if (m.count < PERIOD_SAMPLES) return;
    m.omega = (ln - m.lnN[m.head]) / WINDOW;
  } else {
    m.omega += EMA * ((ln - m.lnN[m.head]) / WINDOW - m.omega);
  }
  m.period = fabsf(m.omega) < OMEGA_MIN ? PERIOD_INF : 1.0f / m.omega;
}

float sur(float period) { return 26.06f / period; }

float logPower(float n) { return log10f(n > N_MIN ? n : N_MIN); }
