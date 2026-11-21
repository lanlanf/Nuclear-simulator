<!-- refreshed: 2026-11-21 -->
# Architecture

**Analysis Date:** 2026-11-21

## System Overview

```text
┌─────────────────────────────────────────────────────────────────────┐
│                     Hosts (two build systems)                         │
├───────────────────────────────┬─────────────────────────────────────┤
│ Firmware (PlatformIO, UNO R4) │ Desktop (CMake)                      │
│ `src/main.cpp`                │ `desktop/sim_cli.cpp` (CSV stdout)   │
│ `src/panel_io.*` `src/display.*`│ `tests/*.cpp` (doctest)            │
└──────────────┬────────────────┴──────────────┬──────────────────────┘
               │ Inputs / Readings              │ Reactor or raw modules
               ▼                                ▼
┌─────────────────────────────────────────────────────────────────────┐
│ Facade: `Reactor` — `lib/reactor/src/reactor.{h,cpp}`                │
└──────────────┬──────────────────────────────────────────────────────┘
               ▼
┌─────────────────────────────────────────────────────────────────────┐
│ Physics modules (free functions on POD structs)                       │
│ `kinetics.*`  `plant.*`  `instruments.*`  `protection.*`  `params.h`  │
└─────────────────────────────────────────────────────────────────────┘
               ▲ validated against (offline)
┌─────────────────────────────────────────────────────────────────────┐
│ Python reference: `tools/reference.py`, `tools/compare.py`            │
│ → `figures/*.png`; `tools/capture_serial.py` → `results/*.csv`        │
└─────────────────────────────────────────────────────────────────────┘
```

## Component Responsibilities

| Component | Responsibility | File |
|-----------|----------------|------|
| Params | Keepin 6-group data, plant constants, trip/warn setpoints, `DOLLAR` | `lib/reactor/src/params.h` |
| Kinetics | Point kinetics: `initEquilibrium`, `stepImplicit` (backward Euler, closed form, 10 substeps when rho ≥ beta), `stepExplicit` (demo only), `totalBeta`, `N_MAX` clamp | `lib/reactor/src/kinetics.{h,cpp}` |
| Plant | Rod S-curve worth, safety bank worth, rate-limited `moveRod`, lumped heat balance `stepTemperature` (in dT = T − Tref), `feedback`, `isValid` | `lib/reactor/src/plant.{h,cpp}` |
| Instruments | `PeriodMeter` (1 s ln(n) ring buffer, EMA on inverse period), `sur`, `logPower` | `lib/reactor/src/instruments.{h,cpp}` |
| Protection | Trip/warn bitmasks, `evaluate`, `warnings`, `tryReset` | `lib/reactor/src/protection.{h,cpp}` |
| Reactor facade | Owns all state; `init()`, `step(Inputs, h)` → `const Readings&`; latch | `lib/reactor/src/reactor.{h,cpp}` |
| Firmware loop | Fixed-step accumulator, serial commands/output, OLED cadence | `src/main.cpp` |
| Panel | Pots (EMA), SCRAM ISR, debounced reset, LEDs, buzzer, rod servo | `src/panel_io.{h,cpp}` |
| Display | SSD1306 128x64 readout; no-op if absent | `src/display.{h,cpp}` |
| Desktop CLI | Scenarios hold/step/ramp (kinetics only), rodpull (no protection), scram/startup (full facade) | `desktop/sim_cli.cpp` |
| Tests | doctest unit tests of each module and the facade | `tests/test_*.cpp`, `third_party/doctest.h` |

## Pattern Overview

**Overall:** Portable core library + thin hardware/host adapters (functional core, imperative shell).

**Key Characteristics:**
- Physics is pure C++11 with no Arduino headers (only `<stdint.h>`, `<math.h>`), so the same sources compile under PlatformIO and CMake.
- Modules are POD structs plus free functions; only `Reactor`, `Panel`, `Display` are classes.
- All arithmetic is `float` (enforced by `-Wdouble-promotion -Wfloat-conversion` in `CMakeLists.txt` and `platformio.ini`); tests may use `double` for references.
- No dynamic allocation; fixed arrays (`MAX_GROUPS = 6`, `PERIOD_SAMPLES = 11`).

## Layers

**Params:**
- Purpose: constants and defaults as `inline` factory functions (`keepinU235()`, `defaultPlant()`, `defaultSetpoints()`)
- Location: `lib/reactor/src/params.h`
- Depends on: nothing. Used by: every module.

**Physics modules:**
- Location: `lib/reactor/src/{kinetics,plant,instruments,protection}.{h,cpp}`
- Depends on: `params.h` only. Do not call each other (except via `params.h`).
- Used by: `reactor.cpp`, `desktop/sim_cli.cpp`, `tests/`, `src/main.cpp` (benchmark only).

**Facade:**
- Location: `lib/reactor/src/reactor.{h,cpp}`
- Composes modules in a fixed order; the only place holding the protection latch.
- Used by: `src/main.cpp`, `desktop/sim_cli.cpp` (scram/startup), `tests/test_reactor.cpp`.

**Shell (hardware/host):**
- Location: `src/`, `desktop/`
- Converts physical/CLI inputs to `Inputs` and renders `Readings`.

## Data Flow

### Primary Path (firmware tick)

1. `loop()` adds elapsed `millis()` to `accMs`, caps at `ACC_CAP_MS = 250` (`src/main.cpp`).
2. While `accMs >= DT_MS (10)` and `steps < MAX_STEPS (10)`: `panel.readInputs()` builds `Inputs` (rod pot EMA → `rodDemand`, coolant pot → `Tcool` 280–300 K, ISR SCRAM flag, debounced reset edge) (`src/panel_io.cpp`).
3. Serial-command state is merged: `overrideOn`, `rhoOverride`, one-shot `softScram`/`softReset`; reset calls `panel.acknowledge()`.
4. `reactor.step(in, 0.010f)` (`lib/reactor/src/reactor.cpp`), in order:
   - a. Manual scram → `latched_ |= TRIP_MANUAL`; reset → `tryReset(latched_, max(rod_, rodDemand), present_)` using the previous step's present trips.
   - b. Rods: if latched, both banks → 0 at `scramSpeed`; else regulating rod → demand and safety bank → 1 at `rodSpeed`.
   - c. Reactivity: override value if test mode and not latched; else `rodWorth + safetyWorth + feedback(dT_)` (previous-step temperature).
   - d. `stepImplicit` kinetics → `stepTemperature` → `update(meter_)`.
   - e. Time: `tWhole_` (uint32 s) + `tFrac_` to avoid float drift.
   - f. Fill `Readings` (t, n, rho, rhoDollars, T, period, sur, logP, rod).
   - g. `evaluate()` present trips, `| TRIP_FAULT` if kinetics failed, mask `OVERRIDE_BYPASS` in test mode, `latched_ |= present`; `trips = latched_`, `warn = warnings()`.
5. Back in `loop()`: any trip clears `overrideOn`; `panel.show(r)` drives LEDs/buzzer/servo every step.
6. After stepping: `handleSerialCommands()`; at 10 Hz `printReadings()` (plotter / CSV / Teleplot); at 5 Hz `display.draw(*last, overrideOn)`.

### Desktop path

1. `desktop/sim_cli.cpp` parses `--scenario --rho-dollars --dt --method --t-end`.
2. Runs module functions directly (hold/step/ramp/rodpull) or the `Reactor` facade (scram/startup), printing CSV every 0.1 s.
3. `tools/compare.py` runs `build/sim_cli(.exe)`, compares with `tools/reference.py` (SciPy), writes `figures/`.

### Serial capture

1. `tools/capture_serial.py` records firmware CSV mode (`m c`) into `results/*.csv`.

**State Management:**
- All simulation state lives in one `Reactor` instance (static in `src/main.cpp`). Shell state is file-static globals in `src/main.cpp` and members of `Panel`.

## Key Abstractions

**`Inputs` / `Readings`:** the only contract between shell and physics (`lib/reactor/src/reactor.h`). `step()` returns a reference to an internal `Readings` valid until the next step.

**Trip bitmask:** `TRIP_MANUAL 1, HIGH_POWER 2, HIGH_TEMP 4, SHORT_PERIOD 8, PROMPT_CRIT 16, FAULT 32`; `WARN_POWER/TEMP/PERIOD` (`lib/reactor/src/protection.h`).

**Param structs:** `KineticsParams`, `PlantParams`, `Setpoints` with default factories (`lib/reactor/src/params.h`).

## Entry Points

**Firmware:** `setup()`/`loop()` in `src/main.cpp` — Serial 115200, `panel.begin()`, `display.begin()`, `reactor.init()`.
**Desktop CLI:** `main()` in `desktop/sim_cli.cpp`.
**Tests:** `tests/test_main.cpp` (doctest main), registered via `add_test(unit_tests)` in `CMakeLists.txt`.
**Python:** `tools/compare.py`, `tools/reference.py`, `tools/capture_serial.py`.

## Fixed-Step Accumulator Loop

- Physics step is always `DT_S = 0.010 s`, independent of loop jitter.
- `accMs` is capped at 250 ms and at most 10 steps run per `loop()`, so a long stall (OLED draw, serial burst) drops time instead of spiralling.
- `now - lastMs` is unsigned, so `millis()` wraparound is safe.
- Output (10 Hz serial, 5 Hz OLED) is decoupled from physics and uses the latest `Readings`.

## Protection Latch

- `latched_` is sticky: `latched_ |= present` each step; only `tryReset` clears it.
- Reset requires all rods in (`max(rod_, rodDemand) < 0.01`, so the pot must be in too) and no trip present (`present_` from the previous step).
- While latched, both banks drive in at `scramSpeed` and override is ignored.
- Override (physics-test) bypasses HIGH_POWER, HIGH_TEMP, SHORT_PERIOD only; PROMPT_CRIT, FAULT, MANUAL still trip. Firmware clears `overrideOn` on any trip.
- `Panel` re-sounds the buzzer when a new trip bit appears (`r.trips & ~lastTrips_`).

## Architectural Constraints

- **Threading:** single-threaded `loop()`; one ISR (`onScram`) sets a `volatile bool` read under `noInterrupts()` in `Panel::readInputs()`.
- **Global state:** file-statics in `src/main.cpp` (`reactor`, `panel`, `display`, mode, override, line buffer, timers) and `scramPressed` in `src/panel_io.cpp`.
- **Portability:** `lib/reactor/src` must not include `<Arduino.h>`; C++11 only.
- **Float only:** in library and firmware code; keep literals `f`-suffixed.
- **Circular imports:** none; `reactor.h` → `instruments.h`, `kinetics.h`, `params.h`; `reactor.cpp` adds `plant.h`, `protection.h`.

## Anti-Patterns

### Putting latch or trip state outside `Reactor`
**What happens:** shell code deciding trips.
**Why it's wrong:** desktop scenarios and tests would no longer exercise the same protection logic.
**Do this instead:** extend `evaluate()` / `Reactor::step()` in `lib/reactor/src/`.

### Variable-dt stepping
**What happens:** passing elapsed `millis()` straight to `step()`.
**Why it's wrong:** breaks determinism versus `sim_cli` and the Python reference.
**Do this instead:** keep the accumulator in `src/main.cpp`.

## Error Handling

**Strategy:** numerical failures become trips, not exceptions.
- `stepImplicit` returns false on non-positive denominator / non-finite n → `TRIP_FAULT`.
- `evaluate()` flags non-finite n or T as `TRIP_FAULT`; `N_MAX` clamps power.
- `Reactor::init` returns false for invalid `PlantParams` (`isValid`).
- `Display::begin` false → draw is a no-op; firmware continues.
- `sim_cli` returns exit code 2 on bad args, 1 if the startup lead-in trips.

## Cross-Cutting Concerns

**Logging:** serial lines prefixed `#` are comments; data in plotter/CSV/Teleplot formats.
**Validation:** `isValid(PlantParams)`; CLI argument checks; Python overlays vs SciPy.
**Units:** every file header states units; formula tags (F2, F7, F14–F20) reference `docs/PLAN.md`.

---

*Architecture analysis: 2026-11-21*
