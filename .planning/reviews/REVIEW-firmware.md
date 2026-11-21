---
phase: firmware
reviewed: 2026-11-20T00:00:00Z
depth: deep
files_reviewed: 6
files_reviewed_list:
  - src/main.cpp
  - src/panel_io.h
  - src/panel_io.cpp
  - src/display.h
  - src/display.cpp
  - platformio.ini
findings:
  critical: 1
  warning: 5
  info: 7
  total: 13
status: issues_found
---

# Firmware Code Review (Arduino UNO R4 Minima)

**Depth:** deep (framework and library sources checked)
**Status:** issues_found

## Summary

The ISR, the atomic hand-off and the millis() arithmetic are correct. The ISR always latches a SCRAM press. The main risk is that `loop()` can stall, so a latched press is never acted on. The R4 USB CDC `write()` spins forever when the port is open but the host is not reading. Smaller stalls come from `benchmark()` and the OLED transfer. Input validation on `r <dollars>` is missing, and a held SCRAM button can be reset away.

## Critical

### FW-01: USB CDC Serial blocks indefinitely when the host is connected but not reading, which stops the SCRAM response
- **Severity:** Critical (needs hardware check)
- **File:** src/main.cpp:87-127 (`printReadings`), also src/main.cpp:43-47, 56, 70
- **Problem:** `loop()` prints about 60-80 bytes every 100 ms. If DTR is asserted and the host stops draining data, the TX FIFO fills and `Serial.print` never returns. Examples: a paused terminal, a crashed plotter, or a Python script that holds the port without reading. Physics stops. The SCRAM ISR still sets `scramPressed`, but nothing consumes it. Rods do not insert, and LEDs, buzzer and servo freeze. A SCRAM press is effectively missed.
- **Evidence:** `framework-arduinorenesas-uno/cores/arduino/usb/SerialUSB.cpp`, `_SerialUSB::write(const uint8_t*, size_t)`. When `tud_cdc_write_available()` is 0, it does `tud_cdc_write_flush(); continue;` with no timeout. It returns early only when `!connected()`, so the "no host" case does not block. The "host open but not reading" case does.
- **Fix:** Never write more than the FIFO can take. Build the line in a buffer first:
  ```cpp
  char buf[96]; int len = snprintf(buf, sizeof buf, ...);   // or format with dtostrf
  if (len > 0 && Serial.availableForWrite() >= len) Serial.write(buf, len);  // else drop this sample
  ```
  Apply the same guard to `benchmark()` and the help/CSV-header prints. Optionally move the physics step and trip handling ahead of any serial I/O, which is already the case.

## Warnings

### FW-02: A held SCRAM button can be reset away, because the trip is edge-only
- **File:** src/panel_io.cpp:23, 47-50; lib/reactor/src/reactor.cpp:33-36
- **Problem:** SCRAM is a FALLING-edge one-shot. While D2 is still held LOW, a reset press clears `TRIP_MANUAL` once the rods are in. The plant can then be restarted with the SCRAM button physically depressed. A real manual trip is level-held.
- **Fix:** In `readInputs()`, use `in.scram = scramPressed || digitalRead(PIN_SCRAM) == LOW;`. Keep the ISR so that sub-10 ms taps are still caught.

### FW-03: `r <dollars>` accepts garbage, NaN, inf and huge values
- **File:** src/main.cpp:58-60
- **Problem:** `strtof("abc")` returns 0, so override quietly turns on at $0. Inputs such as `r nan`, `r inf` or `r 1e30` feed a non-finite or extreme reactivity into `stepImplicit`. Override bypasses the HIGH_POWER, HIGH_TEMP and SHORT_PERIOD trips (reactor.cpp:11), so only PROMPT_CRIT or FAULT can stop the run. A NaN could also poison `k_` state until `init()`.
- **Fix:**
  ```cpp
  char* end; const float d = strtof(cmd + 2, &end);
  if (end == cmd + 2 || *end != '\0' || !isfinite(d) || fabsf(d) > 2.0f) { Serial.println("# bad r"); return; }
  ```

### FW-04: `benchmark()` blocks the control loop
- **File:** src/main.cpp:36-48, 67-68
- **Problem:** The 1000 `stepImplicit` calls run synchronously inside `handleSerialCommands()`. During that time no SCRAM is processed and outputs freeze. Afterwards the accumulator is capped at 250 ms, so simulated time silently slips.
- **Fix:** Allow `b` only while tripped, or run it in chunks of about 50 iterations per `loop()`. At minimum, document the stall and reset `lastMs = millis()` after the benchmark.

### FW-05: The OLED full-frame transfer takes most of the 10 ms budget (needs hardware check)
- **File:** src/main.cpp:157-160, src/display.cpp (`draw` → `display()`)
- **Problem:** Each `display()` sends 1024 data bytes plus framing, in chunks of `WIRE_MAX`. The Adafruit driver switches to `wireClk` (400 kHz) only during the transfer and restores 100 kHz afterwards (`Adafruit_SSD1306.cpp`, `SETWIRECLOCK`/`RESWIRECLOCK`, WIRE_MAX selection at lines 63-72). That is still about 25-30 ms, or about 100 ms if the clock change does not take effect on the R4 Wire. This happens every 200 ms, so SCRAM latency is up to one frame and steps arrive in bursts. If the I2C bus hangs (OLED unplugged at runtime), R4 Wire can block for its timeout.
- **Fix:** Measure `display()` with `micros()`. Call `Wire.setClock(400000)` after `begin()`. Consider drawing only changed lines, or splitting the frame across loops. Since `readInputs` runs at the start of every step, latency is bounded by the transfer time.

### FW-06: The buzzer drive assumes an active buzzer (needs hardware check)
- **File:** src/panel_io.cpp:79-80
- **Problem:** `digitalWrite(PIN_BUZZER, HIGH)` sounds only an active (self-oscillating) buzzer. A passive piezo just clicks once and stays silent. The trip alarm would then be inaudible.
- **Fix:** Confirm the part. If it is passive, use `tone(PIN_BUZZER, 2000)` / `noTone(PIN_BUZZER)` and call them only on state changes, because `tone()` restarts its timer. Check that `tone()` does not take the same FSP timer channel as Servo. Both use `FspTimer::get_available_timer`, so they will get different channels.

## Info

### FW-07: The ISR and atomic hand-off are correct
- **File:** src/panel_io.cpp:22-23, 47-50
- `volatile bool`, a single-byte write in the ISR, and read-and-clear under `noInterrupts()` are correct. D2 is an IRQ-capable pin on the Minima (`digitalPinToInterrupt(2)` is valid). Contact bounce on FALLING only re-sets the flag, which is harmless. Presses between steps coalesce into one, which is fine for a latch.

### FW-08: millis() wrap handled
- **File:** src/main.cpp:142-145, 153, 157; src/panel_io.cpp:57
- All timers use unsigned `now - then`, so they are safe across the 49.7-day wrap. `millis() % 1000` at panel_io.cpp:79 has one shortened beep period at the wrap, which is cosmetic.

### FW-09: Reset debounce fires if the button is held at boot
- **File:** src/panel_io.h:26-28, src/panel_io.cpp:54-62
- `resetRaw_` and `resetStable_` start as false (released). A button held through boot produces a "press" 30 ms after the first step. That is harmless because a reset with no latch is a no-op. Initialize both from `digitalRead` in `begin()` if that matters.

### FW-10: The over-long command line is truncated and then executed
- **File:** src/main.cpp:81-83
- Characters beyond 31 are dropped silently, and the truncated prefix runs on newline. For example, a long `r 0.123456789…` executes with fewer digits. Set an `overflow` flag and reject the line instead. `\r\n` yields an empty second command, which is correctly ignored at line 69.

### FW-11: The `last` pointer is valid
- **File:** src/main.cpp:25, 160, 151
- `reactor.step` returns a reference to the member `Reactor::r_` of a static object, so it is valid for program lifetime. The `nullptr` guard covers only the first loop before any step.

### FW-12: Servo, ADC, pins and memory have no conflicts
- Servo on Renesas is software-timed on a free GPT/AGT channel and toggles the pin in the timer ISR (`.pio/libdeps/uno_r4_minima/Servo/src/renesas/Servo.cpp`, `servo_timer_config`). The code uses no `analogWrite`, so there is no PWM conflict on D9.
- `analogRead` defaults to 10-bit on R4. Dividing by 1023 at panel_io.cpp:25 matches. Call `analogReadResolution(14)` and divide by 16383 for finer coolant resolution if wanted.
- I2C is on A4/A5 (SDA/SCL), and USB is native. Neither collides with A0, A1, D2-D7, D9 or D12.
- The SSD1306 buffer is `malloc(1024)` in `begin()`, which is small relative to 32 KB. A failure is reported through `begin()`, which returns false and is handled.

### FW-13: The soft SCRAM/reset flags work but run one loop late
- **File:** src/main.cpp:30-31, 151-153
- The flags are set after the step loop and consumed in the next loop iteration that runs a step. They are never lost, because the flags are cleared only inside the step loop. The CSV header is printed only on `m c`, so a reconnecting logger misses it. Float printing uses `Print::printFloat`, which prints "ovf" above about 4.29e9 and "nan" for NaN. That is acceptable for these ranges.

---
_Reviewer: gsd-code-reviewer · Depth: deep_
