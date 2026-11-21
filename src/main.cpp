// main.cpp - Arduino UNO R4 Minima reactor simulator firmware.
// Physics runs in fixed 10 ms steps from an accumulator; serial output at 10 Hz,
// OLED at 5 Hz. Units: time s (ms for timers), power normalized, rho $ on serial.
#include <Arduino.h>
#include <stdlib.h>
#include <string.h>

#include "display.h"
#include "kinetics.h"
#include "panel_io.h"
#include "reactor.h"

static const uint32_t DT_MS = 10;
static const float DT_S = 0.010f;
static const uint8_t MAX_STEPS = 10;
static const uint32_t ACC_CAP_MS = 250;
static const uint32_t PRINT_MS = 100;
static const uint32_t DRAW_MS = 200;

enum OutputMode { MODE_PLOTTER, MODE_CSV, MODE_TELEPLOT };

static Reactor reactor;
static Panel panel;
static Display display;
static const Readings* last = nullptr;

static OutputMode mode = MODE_PLOTTER;
static bool overrideOn = false;
static float rhoOverride = 0.0f;  // dk/k
static bool softScram = false;
static bool softReset = false;
static char line[32];
static uint8_t lineLen = 0;
static uint32_t lastMs = 0, accMs = 0, lastPrintMs = 0, lastDrawMs = 0;

static void benchmark() {
  const KineticsParams p = keepinU235();
  Kinetics k;
  initEquilibrium(k, p, 1.0f);
  const uint32_t start = micros();
  for (int i = 0; i < 1000; ++i) stepImplicit(k, p, 0.0f, 0.0f, DT_S);
  const uint32_t us = micros() - start;
  Serial.print("# stepImplicit: ");
  Serial.print(static_cast<float>(us) / 1000.0f, 2);
  Serial.print(" us per step (n = ");
  Serial.print(k.n, 6);
  Serial.println(")");
}

static void handleCommand(const char* cmd) {
  if (cmd[0] == 'm' && cmd[1] == ' ') {
    if (cmd[2] == 'p') mode = MODE_PLOTTER;
    else if (cmd[2] == 't') mode = MODE_TELEPLOT;
    else if (cmd[2] == 'c') {
      mode = MODE_CSV;
      Serial.println("t,rho,n,T,period,sur,rod,trip");
    }
  } else if (cmd[0] == 'r' && cmd[1] == ' ') {
    rhoOverride = strtof(cmd + 2, nullptr) * totalBeta(keepinU235());
    overrideOn = true;
  } else if (!strcmp(cmd, "x")) {
    overrideOn = false;
  } else if (!strcmp(cmd, "s")) {
    softScram = true;
  } else if (!strcmp(cmd, "z")) {
    softReset = true;
  } else if (!strcmp(cmd, "b")) {
    benchmark();
  } else if (cmd[0] != '\0') {
    Serial.println("# commands: m p|m c|m t, r <dollars>, x, s, z, b");
  }
}

static void handleSerialCommands() {
  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());
    if (c == '\n' || c == '\r') {
      line[lineLen] = '\0';
      handleCommand(line);
      lineLen = 0;
    } else if (lineLen < sizeof(line) - 1) {
      line[lineLen++] = c;
    }
  }
}

static void printReadings(const Readings& r) {
  if (mode == MODE_CSV) {
    Serial.print(r.t, 2);
    Serial.print(',');
    Serial.print(r.rhoDollars, 5);
    Serial.print(',');
    Serial.print(r.n, 8);
    Serial.print(',');
    Serial.print(r.T, 3);
    Serial.print(',');
    Serial.print(r.period, 2);
    Serial.print(',');
    Serial.print(r.sur, 4);
    Serial.print(',');
    Serial.print(r.rod, 4);
    Serial.print(',');
    Serial.println(r.trips);
  } else if (mode == MODE_TELEPLOT) {
    Serial.print(">P:");
    Serial.println(100.0f * r.n, 3);
    Serial.print(">logP:");
    Serial.println(r.logP, 3);
    Serial.print(">rho:");
    Serial.println(r.rhoDollars, 4);
    Serial.print(">T:");
    Serial.println(r.T, 2);
    Serial.print(">rod:");
    Serial.println(100.0f * r.rod, 1);
    Serial.print(">SUR:");
    Serial.println(r.sur, 3);
  } else {
    Serial.print("P:");
    Serial.print(100.0f * r.n, 2);
    Serial.print(",rod:");
    Serial.print(100.0f * r.rod, 1);
    Serial.print(",rho_cents:");
    Serial.print(100.0f * r.rhoDollars, 2);
    Serial.print(",T_minus_300:");
    Serial.println(r.T - 300.0f, 2);
  }
}

void setup() {
  Serial.begin(115200);
  const uint32_t start = millis();
  while (!Serial && millis() - start < 2000) {
  }
  panel.begin();
  if (!display.begin()) Serial.println("# no OLED found, continuing without it");
  reactor.init();
  Serial.println("# reactor simulator ready. commands: m p|m c|m t, r <dollars>, x, s, z, b");
  lastMs = millis();
}

void loop() {
  const uint32_t now = millis();
  accMs += now - lastMs;  // unsigned subtraction survives millis() wrap
  lastMs = now;
  if (accMs > ACC_CAP_MS) accMs = ACC_CAP_MS;

  uint8_t steps = 0;
  while (accMs >= DT_MS && steps < MAX_STEPS) {
    Inputs in = panel.readInputs();
    in.overrideOn = overrideOn;
    in.rhoOverride = rhoOverride;
    if (softScram) in.scram = true;
    if (softReset) in.reset = true;
    softScram = softReset = false;
    if (in.reset) panel.acknowledge();

    const Readings& r = reactor.step(in, DT_S);
    if (r.trips != 0) overrideOn = false;  // a trip ends the test; reset returns to the pot
    panel.show(r);
    last = &r;
    accMs -= DT_MS;
    ++steps;
  }

  handleSerialCommands();
  if (last == nullptr) return;
  if (now - lastPrintMs >= PRINT_MS) {
    lastPrintMs = now;
    printReadings(*last);
  }
  if (now - lastDrawMs >= DRAW_MS) {
    lastDrawMs = now;
    display.draw(*last, overrideOn);
  }
}
