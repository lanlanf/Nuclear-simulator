// main.cpp - Arduino UNO R4 (Minima or WiFi) reactor simulator firmware.
// Physics runs in fixed 10 ms steps from an accumulator; serial output at 10 Hz,
// OLED at 5 Hz. Units: time s (ms for timers), power normalized, rho $ on serial.
#include <Arduino.h>
#include <avr/dtostrf.h>
#include <math.h>
#include <stdio.h>
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

// Drops the line if the USB TX buffer can't take it whole; never waits on the host.
static void sendLine(const char* buf, int len) {
  if (len <= 0) return;
  // R4 WiFi (NO_USB): Serial is a UART into the USB bridge, never stalls, and availableForWrite() is always 0.
#ifndef NO_USB
  if (Serial.availableForWrite() < len) return;
#endif
  Serial.write(reinterpret_cast<const uint8_t*>(buf), len);
}

static void sendFormatted(char* buf, int size, int n) {
  if (n < 0) return;
  if (n >= size) n = size - 1;
  sendLine(buf, n);
}

static void sendText(const char* s) { sendLine(s, static_cast<int>(strlen(s))); }

static void sendValue(const char* prefix, float x, int digits) {
  char v[24], buf[160];
  dtostrf(x, 0, digits, v);
  sendFormatted(buf, sizeof buf, snprintf(buf, sizeof buf, "%s%s\r\n", prefix, v));
}

static void benchmark() {
  const KineticsParams p = keepinU235();
  Kinetics k;
  initEquilibrium(k, p, 1.0f);
  const uint32_t start = micros();
  for (int i = 0; i < 1000; ++i) stepImplicit(k, p, 0.0f, 0.0f, DT_S);
  const uint32_t us = micros() - start;
  const uint32_t drawStart = micros();
  display.draw(*last, overrideOn);  // 0 us when no OLED answered
  const uint32_t drawUs = micros() - drawStart;
  char a[24], b[24], buf[160];
  dtostrf(static_cast<float>(us) / 1000.0f, 0, 2, a);
  dtostrf(k.n, 0, 6, b);
  sendFormatted(buf, sizeof buf,
                snprintf(buf, sizeof buf, "# stepImplicit: %s us per step (n = %s), draw: %lu us\r\n",
                         a, b, static_cast<unsigned long>(drawUs)));
  lastMs = millis();
}

static void handleCommand(const char* cmd) {
  if (cmd[0] == 'm' && cmd[1] == ' ') {
    if (cmd[2] == 'p') mode = MODE_PLOTTER;
    else if (cmd[2] == 't') mode = MODE_TELEPLOT;
    else if (cmd[2] == 'c') {
      mode = MODE_CSV;
      sendText("t,rho,n,T,period,sur,rod,trip\r\n");
    }
  } else if (cmd[0] == 'r' && cmd[1] == ' ') {
    char* end;
    const float d = strtof(cmd + 2, &end);
    const bool parsed = end != cmd + 2;
    while (*end == ' ' || *end == '\t') ++end;
    if (!parsed || *end != '\0' || !isfinite(d) || fabsf(d) > 10.0f) {
      sendText("# error: r needs a finite number in [-10, 10] dollars\r\n");
      return;
    }
    rhoOverride = d * totalBeta(keepinU235());
    overrideOn = true;
  } else if (!strcmp(cmd, "x")) {
    overrideOn = false;
  } else if (!strcmp(cmd, "s")) {
    softScram = true;
  } else if (!strcmp(cmd, "z")) {
    softReset = true;
  } else if (!strcmp(cmd, "b")) {
    if (last == nullptr || last->trips == 0) {
      sendText("# error: b only while tripped\r\n");
      return;
    }
    benchmark();
  } else if (cmd[0] != '\0') {
    sendText("# commands: m p|m c|m t, r <dollars>, x, s, z, b\r\n");
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
  char buf[160];
  if (mode == MODE_CSV) {
    char t[24], rho[24], n[24], T[24], per[24], sur[24], rod[24];
    dtostrf(r.t, 0, 2, t);
    dtostrf(r.rhoDollars, 0, 5, rho);
    dtostrf(r.n, 0, 8, n);
    dtostrf(r.T, 0, 3, T);
    dtostrf(r.period, 0, 2, per);
    dtostrf(r.sur, 0, 4, sur);
    dtostrf(r.rod, 0, 4, rod);
    sendFormatted(buf, sizeof buf,
                  snprintf(buf, sizeof buf, "%s,%s,%s,%s,%s,%s,%s,%u\r\n", t, rho, n, T, per,
                           sur, rod, static_cast<unsigned>(r.trips)));
  } else if (mode == MODE_TELEPLOT) {
    sendValue(">P:", 100.0f * r.n, 3);
    sendValue(">logP:", r.logP, 3);
    sendValue(">rho:", r.rhoDollars, 4);
    sendValue(">T:", r.T, 2);
    sendValue(">rod:", 100.0f * r.rod, 1);
    sendValue(">SUR:", r.sur, 3);
  } else {
    char p[24], rod[24], rho[24], T[24];
    dtostrf(100.0f * r.n, 0, 2, p);
    dtostrf(100.0f * r.rod, 0, 1, rod);
    dtostrf(100.0f * r.rhoDollars, 0, 2, rho);
    dtostrf(r.T - 300.0f, 0, 2, T);
    sendFormatted(buf, sizeof buf,
                  snprintf(buf, sizeof buf, "P:%s,rod:%s,rho_cents:%s,T_minus_300:%s\r\n", p,
                           rod, rho, T));
  }
}

void setup() {
  Serial.begin(115200);
  const uint32_t start = millis();
  while (!Serial && millis() - start < 2000) {
  }
  panel.begin();
  if (!display.begin()) sendText("# no OLED found, continuing without it\r\n");
  reactor.init();
  sendText("# reactor simulator ready. commands: m p|m c|m t, r <dollars>, x, s, z, b\r\n");
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
