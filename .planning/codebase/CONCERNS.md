# Codebase Concerns

**Analysis Date:** 2026-11-21

Scope: `lib/reactor/src/` (physics), `src/` (UNO R4 Minima firmware), `desktop/sim_cli.cpp`, `tests/`, `tools/`, build config, and gaps against `docs/PLAN.md` and `docs/CODE-GUIDE.html`.

Items already recorded in `docs/LOG.md` are listed under **Known issues** and not re-argued: firmware never run on hardware, +$0.2 pull overshoot (~81 %) trips high power, 11.1 s minimum startup period vs the 10 s trip, and the dT-based thermal model that avoids float stagnation.

## Tech Debt

**Two different definitions of "one dollar":**
- Issue: `DOLLAR = 0.0065f` in `lib/reactor/src/params.h` sizes the rod and safety worths, while `totalBeta(keepinU235())` (= 0.006502) in `lib/reactor/src/kinetics.cpp` converts rho to dollars for readings, the prompt-critical trip and the `r <dollars>` command in `src/main.cpp`. Tests hard-code `0.0065` (`tests/test_kinetics.cpp`, `tests/test_reactor.cpp`, `tests/test_plant.cpp`).
- Impact: ~0.03 % mismatch. Rod worth at full withdrawal is +$1.0997 instead of +$1.1. Harmless now, but any setpoint near $1 (prompt-critical trip at `rhoDollars >= 1.0f` in `lib/reactor/src/protection.cpp`) depends on which beta is used.
- Fix approach: derive `DOLLAR` from `totalBeta(keepinU235())`, or use one constant everywhere including tests.

**`Reactor::init()` return value ignored:**
- Issue: `reactor.init()` in `src/main.cpp` `setup()` and in `desktop/sim_cli.cpp` discards the `bool`. If the parameters are invalid, the object keeps its default-constructed state (`kp_`, `pp_`, `sp_`, `k_`, `meter_` uninitialized).
- Files: `src/main.cpp`, `desktop/sim_cli.cpp`, `lib/reactor/src/reactor.h`
- Impact: only defaults are used today, so it cannot fail. Any future parameter change (e.g. a tuned `alphaT`) that fails `isValid()` would run on garbage memory.
- Fix approach: check the result; on the board, latch a fault/print an error and halt the physics loop.

**Duplicated rod-position bisection:**
- Issue: `rodFor()` exists in both `desktop/sim_cli.cpp` and `tests/test_plant.cpp` (acknowledged with a `note:` comment).
- Impact: low; drift risk if one copy changes.
- Fix approach: leave as is unless a third use appears.

**README vs build mismatch:**
- Issue: `README.md` says "Needs a C++17 compiler"; `CMakeLists.txt` sets `CMAKE_CXX_STANDARD 11`.
- Fix approach: correct the README (or the standard) so one is authoritative.

**Unpinned Python dependencies:**
- Issue: `tools/requirements.txt` lists `numpy`, `scipy`, `matplotlib`, `pyserial` with no versions.
- Impact: validation figures in `figures/` and the error table in `docs/LOG.md` are not reproducible to a known toolchain.
- Fix approach: pin versions (or add a `requirements.lock`).

**Python venv shares PlatformIO's TLS workaround:**
- Issue: `.venv` holds both the tools and PlatformIO, so `pip-system-certs` (needed for TLS interception on the work network) loads on every interpreter start.
- Files: `.venv` (environment, not source)
- Impact: low. Checked on 2026-11-21: Python starts and imports numpy/scipy in 1.8 s. An earlier "hang" turned out to be parallel agents sharing one terminal.
- Fix approach: none needed. Optionally split PlatformIO into its own venv.

## Known Bugs

**Known issues carried from `docs/LOG.md` (not re-analyzed):**
- Firmware not yet run on hardware.
- +$0.2 rod pull from full power peaks at n ≈ 3.39 and trips HIGH POWER (1.20).
- Startup scenario minimum period 11.1 s vs 10 s short-period trip (~10 % margin).
- Thermal model integrates dT to avoid float stagnation at ~320 K.

**Coolant pot's lower range trips the reactor at steady state:**
- Symptoms: turning the coolant pot below about 286 K with the rod at critical drives power to `1 + (290 − Tcool)·K` → e.g. 1.33 at 280 K, past `powerTrip = 1.20`.
- Files: `src/panel_io.cpp` (`TCOOL_MIN = 280`, `TCOOL_SPAN = 20`), `lib/reactor/src/params.h` (`defaultSetpoints`)
- Trigger: overcooling demo from PLAN §2.4 ("lower Tcool by 10 K") on the real panel. `tests/test_plant.cpp` T18 runs this without protection, so the tests do not catch that the panel trips.
- Workaround: narrow the pot range or reduce the rod first. Decide whether the trip is the intended teaching point and document it.

**Reactor starts at 100 % power regardless of panel state:**
- Symptoms: `Reactor::init()` always sets n = 1, rod = 0.5, safety bank out. `Panel::begin()` reads the actual pot position. If the rod pot is near full-out at power-on, the rod withdraws toward +$1.1 and trips within seconds; if the coolant pot is low, see above.
- Files: `lib/reactor/src/reactor.cpp`, `src/main.cpp` `setup()`, `src/panel_io.cpp` `begin()`
- Workaround: center both pots before power-up.
- Fix approach: start tripped/shut down (as `sim_cli --scenario startup` does) or require the rod pot in before running.

**`sim_cli --dt` larger than 0.1 s breaks the period meter:**
- Symptoms: `update()` in `lib/reactor/src/instruments.cpp` takes at most one sample per call and subtracts `SAMPLE_DT` once, so `m.acc` grows without bound when `h > 0.1 s`; the sample spacing no longer equals `WINDOW / 10` and period/SUR are wrong.
- Files: `lib/reactor/src/instruments.cpp`, `desktop/sim_cli.cpp` (accepts any `dt > 0`)
- Trigger: `sim_cli --scenario step --dt 0.2`
- Fix approach: reject `dt > 0.1` in the CLI, or loop the sampler.

**SCRAM release does not hold the trip:**
- Symptoms: the SCRAM input is edge-only (`FALLING` ISR sets a one-shot flag). A reset is accepted while the SCRAM button is still physically held, because `evaluate()` never reports `TRIP_MANUAL` as "present".
- Files: `src/panel_io.cpp`, `lib/reactor/src/protection.cpp` `tryReset()`
- Fix approach: read the D2 level in `readInputs()` and treat a held button as a present trip.

## Real-Time & Embedded Risks

**No overrun detection; time is silently dropped:**
- Problem: `loop()` in `src/main.cpp` caps the accumulator at `ACC_CAP_MS = 250` and runs at most `MAX_STEPS = 10` per pass. Any stall longer than 250 ms (or sustained load above 100 %) throws away simulated time with no counter, flag or serial message.
- CODE-GUIDE M6 explicitly requires "check the step counter against `millis()`" — no step counter exists.
- Fix approach: keep a `uint32_t steps` and a dropped-ms counter; print both in CSV/benchmark.

**OLED full-frame I²C write in the loop:**
- Problem: `Display::draw()` (`src/display.cpp`) calls `clearDisplay()` and `display()` (1 KB over I²C) every 200 ms, blocking. At 400 kHz this is ~25 ms; if the Wire clock falls back to 100 kHz it is ~90–100 ms, which pushes the accumulator toward the 10-step limit. Unmeasured.
- Fix approach: time `draw()` with `micros()` on the board; reduce the redraw rate or update only changed lines if it is large.

**Serial output may block when no host is reading:**
- Problem: `printReadings()` runs unconditionally at 10 Hz. On the R4 Minima the USB CDC `Serial` behavior with an open-but-unread or disconnected port is unverified; a blocking write would stall physics.
- Files: `src/main.cpp`
- Fix approach: test with the monitor closed; guard with `Serial.availableForWrite()` if needed.

**Benchmark disturbs the simulation:**
- Problem: serial command `b` runs 1000 `stepImplicit` calls inline in `handleSerialCommands()`; the elapsed time is then absorbed (up to 250 ms) by the accumulator or dropped.
- Files: `src/main.cpp` `benchmark()`
- Fix approach: acceptable for a lab tool; document that `b` perturbs real time, or run it only while tripped.

**SCRAM ISR has no debounce or noise filter:**
- Problem: `onScram()` in `src/panel_io.cpp` fires on any falling edge on D2 with only the internal pull-up. Servo current spikes or long button leads can produce a spurious SCRAM (fail-safe, but confusing — PLAN §4.4 calls it a "phantom SCRAM").
- Fix approach: RC filter (e.g. 10 kΩ/100 nF) on D2, short twisted leads, and confirm with the board.

**Reset debounce depends on step cadence:**
- Problem: debounce in `Panel::readInputs()` is evaluated only when a physics step runs, and several steps can run back-to-back with the same `millis()`. Works at 10 ms, but couples input handling to physics catch-up.
- Files: `src/panel_io.cpp`

**Unvalidated serial input:**
- Problem: `r <dollars>` uses `strtof` without checking the end pointer; `r abc` silently sets $0 and enables test mode. Lines longer than 31 chars are silently truncated (`line[32]`). `r nan`/`r inf` are caught only indirectly via the `TRIP_FAULT` path in `stepImplicit()`.
- Files: `src/main.cpp` `handleCommand()`, `handleSerialCommands()`
- Fix approach: reject unparsable or non-finite values with a `#` error line.

**Unverified platform assumptions:**
- RA4M1 FPU presence and µs/step are unmeasured (PLAN §11, CODE-GUIDE §14). Timing budget in PLAN depends on it.

## Numerical Fragility

**First-order solver with no sub-stepping below prompt critical:**
- Problem: `stepImplicit()` (`lib/reactor/src/kinetics.cpp`) uses 10 substeps only when `rho >= beta`. Between ~$0.5 and $1 it runs a single 10 ms backward-Euler step. LOG shows 9.9e-3 relative error at +$0.5; error at $0.8–$0.99 is unmeasured and grows as the period shortens toward the 10 s trip region and below.
- Fix approach: add $0.8/$0.95 cases to `tools/compare.py`; consider substeps scaled by `(rho/beta)` or Crank–Nicolson (PLAN §3 optional upgrade).

**Power clamp `N_MAX = 1e6` distorts precursors:**
- Problem: the clamp in `stepImplicit()` feeds the clamped `n` into the precursor update, so precursor inventory is wrong after a clamp. Only reachable past prompt critical, where the reactor is already tripping.
- Files: `lib/reactor/src/kinetics.h`, `lib/reactor/src/kinetics.cpp`

**Float time base:**
- Problem: `tFrac_ += h` in `Reactor::step()` accumulates 0.01 f rounding (0.01 is not exact in binary). Split `tWhole_`/`tFrac_` prevents loss of resolution but each second carries ~1e-7 s drift. Negligible for a demo; noted for long runs.
- Files: `lib/reactor/src/reactor.cpp`

**Period meter thresholds:**
- Problem: `OMEGA_MIN = 1e-4` reports `PERIOD_INF` for |period| > 10 000 s; the first 1 s after init/reset reports infinity (ring buffer filling). The meter is not reset on trip reset, so the 1 s window straddles a discontinuity. EMA weight 0.2 per 0.1 s sample adds ~0.5 s lag to the short-period trip.
- Files: `lib/reactor/src/instruments.cpp`
- Impact: with only ~10 % margin on the startup period (LOG), meter lag/noise decides whether a startup trips.

**Blue "critical" LED threshold near ADC resolution:**
- Problem: `critical` in `Panel::show()` requires `|rho| < 5e-5` (≈ 0.77 ¢). One rod-pot LSB at mid-stroke is ≈ 0.43 ¢ of reactivity (peak differential worth 2·$2.2/stroke ÷ 1023). EMA (`POT_EMA = 0.1`) plus the rod rate limit smooth it, but LED flicker near critical is likely. Same threshold in `stateText()` in `src/display.cpp` (two copies of `5e-5`).

## Hardware / Wiring Risks

**Active buzzer on a GPIO pin:**
- Risk: `PIN_BUZZER = 4` is driven directly. Typical 5 V active piezo buzzers draw 20–30 mA, well over the R4 Minima's 8 mA per-pin limit (PLAN §4.1).
- Files: `src/panel_io.cpp`, wiring table in `README.md`
- Recommendation: drive through an NPN/MOSFET, or verify the buzzer current.

**LED resistor value not specified in README:**
- Risk: `README.md` wiring says only "use a series resistor"; PLAN §4.4 requires ~1 kΩ on the R4 to stay under 8 mA. A 220 Ω R3-style resistor gives ~14 mA per pin.
- Recommendation: state 1 kΩ in the README wiring table.

**Servo supply and brownout:**
- Risk: `servo_.attach(PIN_SERVO)` in `Panel::begin()` energizes the servo at boot; SG90 stall current can brown out the board if powered from the R4 5 V pin (forbidden by the R4 manual, PLAN §4.4). A brownout reset restarts the reactor at 100 % (see Known Bugs).
- Recommendation: separate 5 V supply, common ground, 470–1000 µF bulk cap — documented in README but untested.

**OLED is optional at runtime but the bus is not:**
- Risk: `Display::begin()` failing is handled, but a miswired/shorted I²C bus can hang `Wire` transactions inside `draw()` with no timeout.
- Files: `src/display.cpp`

**Pot wiring direction:**
- Risk: `rodDemand = analogRead/1023` assumes the pot is wired so full-clockwise = withdrawn. A reversed pot inverts rod control and makes the reset interlock (rod demand < 0.01) require the opposite stop.
- Files: `src/panel_io.cpp`

## Fragile Areas

**Trip/reset logic in `Reactor::step()`:**
- Files: `lib/reactor/src/reactor.cpp`, `lib/reactor/src/protection.cpp`
- Why fragile: reset uses `present_` from the previous step and `max(rod_, in.rodDemand)`; override bypass masks three trips via `OVERRIDE_BYPASS`; firmware separately clears `overrideOn` on any trip in `src/main.cpp`. Three places must agree on test-mode semantics.
- Safe modification: extend `tests/test_reactor.cpp` T13/T14 and the override test before changing order of operations.
- Test coverage: no test for reset being refused while a trip condition is still present (e.g. high temperature after a power trip).

**`Readings` returned by reference:**
- Files: `lib/reactor/src/reactor.h`, `src/main.cpp` (`static const Readings* last`)
- Why fragile: `last` points at `reactor.r_`; safe only because there is one global `Reactor`. Copying the reactor or re-initializing mid-loop changes what `last` shows.

**Firmware has zero automated tests:**
- Files: `src/main.cpp`, `src/panel_io.cpp`, `src/display.cpp`
- Why fragile: command parsing, debounce, buzzer/ack logic, accumulator loop and `stateText()` priority are untested and have never run on hardware.

## Scaling Limits

**Real-time budget:**
- Current capacity: 100 Hz physics, 10 Hz serial, 5 Hz OLED. RAM 16 % (5.2 kB), flash 29 % (76 kB) per LOG.
- Limit: unknown — µs/step not yet measured. Stretch goals (two-node feedback, xenon with time acceleration — CODE-GUIDE §12) multiply step cost.
- Scaling path: measure with `b`, add the overrun counter above before adding physics.

**Committed/large artifacts:**
- `results/*.csv` (up to ~3 MB) are git-ignored; `figures/*.png` and `third_party/doctest.h` (363 kB) are committed. Fine at current size.

## Dependencies at Risk

**Arduino libraries unpinned:**
- Risk: `platformio.ini` `lib_deps` (`adafruit/Adafruit SSD1306`, `Adafruit GFX Library`, `arduino-libraries/Servo`) and `platform = renesas-ra` have no version pins.
- Impact: a library update can change behavior (e.g. SSD1306 I²C clock, Servo timer use on R4) or warnings.
- Migration plan: pin versions with `@^x.y.z`.

**Vendored doctest:**
- Risk: `third_party/doctest.h` version not recorded. Low risk.

## Missing Critical Features (vs `docs/PLAN.md` and `docs/CODE-GUIDE.html`)

- **Hardware bring-up and integration (CODE-GUIDE M4–M7):** not started; LOG "Next" items open.
- **Step counter vs `millis()` check (M6):** not implemented.
- **Board CSV vs SciPy comparison (M7, PLAN V5):** `tools/capture_serial.py` logs board CSV but `tools/compare.py` only compares `sim_cli`; no board-vs-reference/board-vs-sim tool.
- **Explicit state machine (PLAN §5: SHUTDOWN → STARTUP → CRITICAL/POWER → TRIPPED → RESET):** implicit only; reactor boots into POWER.
- **Rod-withdrawal interlock on short period (PLAN §9):** not implemented — only a trip at 10 s; no block on withdrawal at the 30 s warning.
- **Separate acknowledge:** PLAN lists "latched annunciators that need an acknowledge"; reset button both acknowledges and resets, and there is no way to silence the buzzer without attempting reset.
- **Log-power bar graph (optional, PLAN §4.2):** not implemented.
- **Wokwi R3 prototype phase (PLAN §7 step 2):** skipped; no R3 build env in `platformio.ini`, so the planned R3 vs R4 µs/step comparison cannot be made.
- **1/M startup figure, doubling time readout (CODE-GUIDE F23/F25, optional):** `sim_cli` emits `inv_m`, but no plotting in `tools/`.
- **Overshoot decision:** LOG leaves open whether to reduce MC or keep the 81 % overshoot; PLAN V8 criteria were amended to match.

## Test Coverage Gaps

**Firmware (`src/`):**
- What's not tested: everything — serial parser, output modes, debounce, buzzer ack, LED logic, OLED text width.
- Risk: first hardware run discovers all bugs at once.
- Priority: High

**Panel-level protection scenarios:**
- What's not tested: overcooling with protection on; reset refused while a trip is still present; reset with SCRAM held; startup from power-on pot positions.
- Files: `tests/test_reactor.cpp`
- Priority: High

**Solver accuracy between $0.5 and $1:**
- What's not tested: error vs SciPy at $0.8–$0.99 without substeps.
- Files: `tests/test_kinetics.cpp`, `tools/compare.py`
- Priority: Medium

**Period meter edge cases:**
- What's not tested: `h > SAMPLE_DT`, meter behavior across a trip reset, response lag vs the 10 s trip.
- Files: `tests/test_reactor.cpp` (T15 covers only steady exponentials)
- Priority: Medium

**CLI argument handling:**
- What's not tested: odd argument counts (`for (i = 1; i + 1 < argc; i += 2)` silently ignores a trailing flag), bad `--method` values (any non-`explicit` string runs implicit).
- Files: `desktop/sim_cli.cpp`
- Priority: Low

---

*Concerns audit: 2026-11-21*
