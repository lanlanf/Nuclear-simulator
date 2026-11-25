// panel_io.cpp - front panel I/O.
// Units: rod demand 0..1, coolant temperature K, times ms.
#include "panel_io.h"

#include <math.h>

static const uint8_t PIN_SCRAM = 2;  // button to GND, interrupt
static const uint8_t PIN_RESET = 3;  // button to GND, debounced in software
static const uint8_t PIN_BUZZER = 4;
static const uint8_t PIN_GREEN = 5;
static const uint8_t PIN_YELLOW = 6;
static const uint8_t PIN_RED = 7;
static const uint8_t PIN_SERVO = 9;
static const uint8_t PIN_BLUE = 12;
static const uint8_t PIN_ROD_POT = A0;
static const uint8_t PIN_COOL_POT = A1;

static const float POT_EMA = 0.1f;
static const float TCOOL_MIN = 285.0f;  // coolant pot range (K), 290 K at mid-scale; 284 K would trip at power
static const float TCOOL_SPAN = 10.0f;
static const uint32_t DEBOUNCE_MS = 30;

static volatile bool scramPressed = false;
static void onScram() { scramPressed = true; }

static float readPot(uint8_t pin) { return static_cast<float>(analogRead(pin)) / 16383.0f; }

void Panel::begin() {
  pinMode(PIN_SCRAM, INPUT_PULLUP);
  pinMode(PIN_RESET, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_SCRAM), onScram, FALLING);
  const uint8_t outputs[] = {PIN_BUZZER, PIN_GREEN, PIN_YELLOW, PIN_RED, PIN_BLUE};
  for (uint8_t pin : outputs) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
  }
  servo_.attach(PIN_SERVO);
  analogReadResolution(14);
  rodPot_ = readPot(PIN_ROD_POT);
  coolPot_ = readPot(PIN_COOL_POT);
}

Inputs Panel::readInputs() {
  rodPot_ += POT_EMA * (readPot(PIN_ROD_POT) - rodPot_);
  coolPot_ += POT_EMA * (readPot(PIN_COOL_POT) - coolPot_);

  Inputs in;
  in.rodDemand = rodPot_;
  in.Tcool = TCOOL_MIN + TCOOL_SPAN * coolPot_;

  noInterrupts();
  in.scram = scramPressed;
  scramPressed = false;
  interrupts();
  if (digitalRead(PIN_SCRAM) == LOW) in.scram = true;  // held button keeps SCRAM asserted

  const uint32_t now = millis();
  const bool raw = digitalRead(PIN_RESET) == LOW;
  if (raw != resetRaw_) {
    resetRaw_ = raw;
    resetChangedMs_ = now;
  }
  if (now - resetChangedMs_ >= DEBOUNCE_MS && raw != resetStable_) {
    resetStable_ = raw;
    in.reset = raw;  // press edge only
  }
  return in;
}

void Panel::show(const Readings& r) {
  const bool tripped = r.trips != 0;
  if ((r.trips & ~lastTrips_) != 0) acked_ = false;  // a new trip sounds again
  lastTrips_ = r.trips;

  const bool critical = !tripped && fabsf(r.rho) < 5.0e-5f && fabsf(r.period) > 1000.0f;
  digitalWrite(PIN_RED, tripped ? HIGH : LOW);
  digitalWrite(PIN_YELLOW, !tripped && r.warn ? HIGH : LOW);
  digitalWrite(PIN_GREEN, !tripped && !r.warn ? HIGH : LOW);
  digitalWrite(PIN_BLUE, critical ? HIGH : LOW);

  const bool beep = tripped ? !acked_ : r.warn != 0 && millis() % 1000 < 100;
  digitalWrite(PIN_BUZZER, beep ? HIGH : LOW);

  const int angle = static_cast<int>(180.0f * r.rod + 0.5f);
  if (abs(angle - lastAngle_) >= 1) {
    servo_.write(angle);
    lastAngle_ = angle;
  }
}
