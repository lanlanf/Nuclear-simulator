# Coding Conventions

**Analysis Date:** 2026-11-21

Scope: C++11 physics library `lib/reactor/src/`, Arduino firmware `src/`, desktop CLI `desktop/sim_cli.cpp`, tests `tests/`, Python tools `tools/`. Normative source for most rules: `docs/CODE-GUIDE.html` (§8 and the "rules" table).

## Naming Patterns

**Files:**
- `snake_case` `.h`/`.cpp` pairs, one module per pair: `lib/reactor/src/kinetics.{h,cpp}`, `plant`, `instruments`, `protection`, `reactor`; `params.h` is header-only (inline factory functions).
- Firmware: `src/panel_io.{h,cpp}`, `src/display.{h,cpp}`, `src/main.cpp`.
- Tests: `tests/test_<module>.cpp` (`test_kinetics.cpp`, `test_plant.cpp`, `test_reactor.cpp`), plus `tests/test_main.cpp` (doctest main only).
- Python: `tools/snake_case.py` (`reference.py`, `compare.py`, `capture_serial.py`).

**Functions:**
- Free functions in `lowerCamelCase`: `stepImplicit`, `initEquilibrium`, `totalBeta`, `moveRod`, `stepTemperature`, `initPeriodMeter`, `tryReset`, `logPower`.
- Physics functions take the state struct by non-const reference first, params by const reference, then scalars, then step `h` last:
  `bool stepImplicit(Kinetics& k, const KineticsParams& p, float rho, float S, float h);`
- Pure helpers return the new value instead of mutating: `float moveRod(float x, float demand, float speed, float h);`, `float stepTemperature(float dT, float P, const PlantParams& p, float h);`
- Parameter factories: `keepinU235()`, `defaultPlant()`, `defaultSetpoints()` in `lib/reactor/src/params.h`.
- File-local helpers are `static` (e.g. `static void benchmark()` in `src/main.cpp`, `static float rodFor(...)` in tests and CLI).

**Variables:**
- Physics symbols keep their textbook letters and case, even when that breaks camelCase: `n`, `C[i]`, `rho`, `S`, `h`, `Lambda` (generation time) vs `lambda[i]` (decay constants), `MC`, `K`, `Tcool`, `Tref`, `alphaT`, `rhoW`, `dT`. Do not rename these to "descriptive" names; they map 1:1 to formulas F1–F20 in `docs/CODE-GUIDE.html`.
- Private members use a trailing underscore: `kp_`, `pp_`, `latched_`, `tWhole_` (`lib/reactor/src/reactor.h`), `oled_`, `ok_` (`src/display.h`), `servo_`, `acked_` (`src/panel_io.h`).
- Constants are `UPPER_SNAKE_CASE`, typed `const`, never `#define`: `MAX_GROUPS`, `DOLLAR`, `N_MAX`, `PERIOD_INF`, `TRIP_FAULT`, `DT_MS`. File-local constants are `static const` (`static const float SAMPLE_DT = 0.1f;` in `instruments.cpp`, `PIN_*` in `src/panel_io.cpp`).
- Python mirrors C++ constants in `UPPER_SNAKE_CASE` (`BETA_I`, `GEN_TIME`, `RHO_W`, `ALPHA_T` in `tools/reference.py`) with the comment "Keep the constants in sync with lib/reactor/src/params.h".

**Types:**
- `PascalCase` plain aggregates (`struct`) for state and parameters: `KineticsParams`, `Kinetics`, `PlantParams`, `Setpoints`, `PeriodMeter`, `Inputs`, `Readings`.
- `class` only where there is hidden state and a lifecycle: `Reactor` (facade), `Panel`, `Display`.
- Unscoped `enum` with prefixed values in firmware: `enum OutputMode { MODE_PLOTTER, MODE_CSV, MODE_TELEPLOT };`
- Bit flags are `const uint8_t` powers of two, not enums: `TRIP_MANUAL = 1 … TRIP_FAULT = 32`, `WARN_POWER = 1 … WARN_PERIOD = 4` (`lib/reactor/src/protection.h`).

## Code Style

**Formatting:**
- No formatter config committed (no `.clang-format`). Observed style is Google-like; match it by hand:
  - 2-space indent, `{` on the same line, ~100-column lines.
  - Access specifiers indented one space: ` public:` / ` private:`.
  - Short bodies on one line: `float feedback(float dT, const PlantParams& p) { return p.alphaT * dT; }`; single-statement `if`/`for` without braces: `if (n > N_MAX) n = N_MAX;`.
  - Pointer/reference binds to the type: `const Readings& r`, `char** argv`.
  - Prefer `++i`.
  - `!strcmp(a, b)` for string equality (`src/main.cpp`, `desktop/sim_cli.cpp`).
- Python: PEP 8, module docstring with units and usage examples (`tools/reference.py`).

**Linting / warning flags:**
- The same flags everywhere for project code: `-Wall -Wextra -Wdouble-promotion -Wfloat-conversion -Wshadow`
  - `CMakeLists.txt`: `set(WARNINGS ...)` applied `PRIVATE` to `reactor` and `sim_cli`.
  - `platformio.ini`: `build_src_flags` (src only, because the Arduino core is not warning-clean).
  - `unit_tests` uses only `-Wall -Wextra -Wshadow`: tests compute references in `double` on purpose. `third_party/` is included as `SYSTEM` so doctest warnings are suppressed.
- Build must be zero-warning (CODE-GUIDE rule: "Build with zero warnings").
- C++11 strict: `CMAKE_CXX_STANDARD 11`, `CMAKE_CXX_EXTENSIONS OFF`. Do not use C++14+ features (no `std::make_unique`, generic lambdas, `constexpr` with loops).

## Float Discipline

Required in `lib/reactor/src/`, `src/`, and the physics path of `desktop/`:
- `float` only. The UNO R4 Cortex-M4 FPU is single precision; `double` is software-emulated. `lib/reactor/src/` contains zero `double` uses; keep it that way.
- Every literal carries `f`: `0.5f`, `1.0e-4f`, `6.2831853f`. A bare `0.5` triggers `-Wdouble-promotion`.
- Use the `f` math functions: `sinf`, `expf`, `logf`, `log10f`, `fabsf` (`<math.h>`, not `<cmath>`).
- Explicit conversions with `static_cast<float>(...)` / `static_cast<uint8_t>(...)`, e.g. `static_cast<float>(substeps)` in `kinetics.cpp`.
- When passing a float to a variadic function, cast to `double` explicitly: `printf("%.6g", static_cast<double>(n))` (`desktop/sim_cli.cpp`).
- Precision hazards are handled structurally, with a one-line comment explaining why:
  - Temperature is integrated as the deviation `dT = T - Tref`, not absolute `T` (`plant.h`: "At T = 320 K a float step of 10 ms rounds away any power imbalance under 0.8 %").
  - Time is `uint32_t tWhole_` + `float tFrac_` (`reactor.h`: "float alone loses 10 ms steps after an hour").
  - Period meter smooths `omega = 1/period`, samples `ln n` every 0.1 s over a 1 s window instead of per-step ratios (`instruments.h`).
  - Clamps: `N_MAX = 1.0e6f` on power; `N_MIN = 1.0e-30f` before `logf`/`log10f`; `PERIOD_INF = 1.0e9f` sentinel.
- Never compare floats with `==` except against an exact sentinel the code assigned (`steady.period == PERIOD_INF`).
- `desktop/sim_cli.cpp` keeps the time accumulator `t` in `double` (desktop only, CSV output); physics state stays `float`.

## Units-in-Comments Convention

- Every source file starts with a two-part header comment: line 1 `// <file> - <one-line purpose>.`, line 2+ `// Units: ...` listing the unit of every quantity in that file. Examples:
  - `// kinetics.h - six-group point kinetics with an external source.`
    `// Units: time s, n normalized power, C_i in the same units as n, rho dk/k, S 1/s.`
  - `// main.cpp - ... Units: time s (ms for timers), power normalized, rho $ on serial.`
- Struct fields carry a trailing unit comment: `float lambda[MAX_GROUPS];  // precursor decay constant (1/s)`, `float rhoOverride = 0.0f;   // dk/k`.
- Reactivity is `dk/k` internally everywhere; dollars appear only at boundaries and the name says so: `rhoDollars` (`Readings`), `--rho-dollars` (CLI), `r <dollars>` serial command (converted with `* totalBeta(keepinU235())`). Use `DOLLAR = 0.0065f` from `params.h` to convert.
- Rod positions: `0 = fully in, 1 = fully withdrawn` (stated in `plant.h`, `reactor.h`).
- Python tools: units in the module docstring.

## Import Organization

**Order** (blank line between groups):
1. Own header first in a `.cpp` (`#include "kinetics.h"`).
2. C system headers in `<x.h>` form: `<math.h>`, `<stdint.h>`, `<stdio.h>`, `<stdlib.h>`, `<string.h>` (not `<cmath>`/`<cstdint>`). Firmware puts `<Arduino.h>` first.
3. Project headers in quotes, alphabetical: `"display.h"`, `"kinetics.h"`, `"panel_io.h"`, `"reactor.h"`.

**Include guards:** `#ifndef REACTOR_<MODULE>_H` in the library (`REACTOR_KINETICS_H`), `<MODULE>_H` in firmware (`PANEL_IO_H`, `DISPLAY_H`). No `#pragma once`. `#endif` with no trailing comment.

**Path aliases:** None. Library headers are found via `target_include_directories(reactor PUBLIC lib/reactor/src)` (CMake) and PlatformIO's `lib/` auto-discovery. Include by bare filename.

**Dependency direction:** `params.h` ← `kinetics.h`, `plant.h`, `protection.h`; `instruments.h` standalone; `reactor.h` includes `instruments.h`, `kinetics.h`, `params.h` and (in `.cpp`) `plant.h`, `protection.h`. The library never includes `<Arduino.h>` or calls `millis()`/`delay()`; time enters only as `h`.

## Header Layout

Pattern for `lib/reactor/src/*.h`:
1. File/purpose comment + `// Units:` line.
2. Include guard.
3. Includes.
4. Module constants (`const float N_MAX = 1.0e6f;  // why`).
5. Structs (plain aggregates, unit comment per field; default member initializers only where a sane default exists, e.g. `Inputs`, `Reactor`).
6. Function declarations, each with a short comment giving the formula ID from CODE-GUIDE (`(F7)`, `(F14)`, `(F18)`) and any contract (return meaning, preconditions).
7. `#endif`.

`.cpp` files: header comment, own include, system includes, `static const` file constants, then definitions in declaration order.

## Error Handling

**Patterns:**
- No exceptions, no `assert`, no heap, no RTTI (embedded target). `lib/reactor/src/` has no `assert(`.
- **bool returns** signal recoverable failure; callers decide:
  - `bool stepImplicit(...)` returns `false` if the denominator is not `> 0` (`if (!(den > 0.0f)) return false;` – written to also catch NaN) or `!isfinite(k.n)`.
  - `bool Reactor::init(...)` returns `false` and keeps the old state when `isValid(plant)` fails (positive `alphaT`, non-positive `MC`/`K`).
  - `bool isValid(const PlantParams&)`, `bool tryReset(uint8_t& latched, ...)`, `bool Display::begin()` (false = no OLED; `draw()` then no-ops).
- **TRIP_FAULT** converts numerical failure into a plant trip instead of a crash: `evaluate()` sets it when `!isfinite(n) || !isfinite(T)`; `Reactor::step` ORs it in when `stepImplicit` returned false. Trips latch in `latched_` and drive both rod banks in at `scramSpeed`.
- **Clamp, don't fail** for expected out-of-range inputs: `moveRod` clamps demand to `[0, 1]`; `n` clamped to `N_MAX`; `logf` arguments floored at `N_MIN`.
- Trip bypass is explicit and narrow: `OVERRIDE_BYPASS = TRIP_HIGH_POWER | TRIP_HIGH_TEMP | TRIP_SHORT_PERIOD` in `reactor.cpp`; `TRIP_PROMPT_CRIT`, `TRIP_FAULT`, `TRIP_MANUAL` are never bypassed.
- CLI: bad arguments → `usage()` to `stderr` and non-zero exit. Firmware: unknown serial command → one `#`-prefixed help line.
- Note: CODE-GUIDE describes `stepImplicit` as `void` that "asserts" the denominator; the code deliberately returns `bool` instead. Follow the code.

## Logging

**Framework:** None in the library (pure functions, no I/O).
- Firmware: `Serial.print*` at 115200 baud; comment/diagnostic lines start with `# ` so plotters and CSV capture ignore them (`src/main.cpp`). Data rows at 10 Hz in plotter/CSV/Teleplot mode.
- CLI: CSV rows on `stdout` via `printf`, errors on `stderr`.
- Tests: `MESSAGE(...)` to record measured values (e.g. T12 overshoot).

## Comments

**When to Comment:**
- File header + units (always).
- Formula/plan IDs that tie code to the spec: `(F2)`, `(F7)`, `(V8)`, `T12`.
- The *why* behind a numerical choice, in one line: `// power clamp, keeps float finite above prompt critical`, `// The rod pot must be in too, or the rod would run out on its own after reset.`
- Duplications that are intentional get a `// note:` comment (`desktop/sim_cli.cpp` `rodFor`: "note: same bisection as tests/test_plant.cpp").
- Numbered step comments in the facade's step order: `// 1. Rods`, `// 2. Reactivity`, `// 3-5. Kinetics, heat balance, period meter`, `// 6. Trips latch.`

**JSDoc/TSDoc:** Not used. Plain `//` comments above declarations; no Doxygen tags.

## Function Design

**Size:** Short; most library functions are 1–25 lines. Largest: `Reactor::step` (~55 lines), `main()` in `desktop/sim_cli.cpp`.

**Parameters:** State by `&`, params by `const&`, scalars by value; physics step size `h` always passed in, never read from a clock. Default arguments only on the facade (`Reactor::init(const PlantParams& plant = defaultPlant(), const Setpoints& sp = defaultSetpoints())`).

**Return Values:** New value for pure updates (`moveRod`, `stepTemperature`); `bool` success for fallible ops; bitmask `uint8_t` for trips/warnings; `const Readings&` to an internal buffer from `Reactor::step` (valid until the next call).

## Module Design

**Exports:** All non-`static` functions in a library `.h` are the public API; file-private helpers and constants are `static` in the `.cpp`. No namespaces.

**Barrel Files:** None. `reactor.h` is the facade used by `src/main.cpp`, `src/panel_io.h`, `src/display.h`, and the CLI's `scram`/`startup` scenarios; lower-level headers are included directly where needed (CLI `hold/step/ramp/rodpull`, tests).

**Commits:** Conventional Commits (`feat(lib):`, `feat(firmware):`, `test:`, `docs:`, `chore:`), per CODE-GUIDE.

---

*Convention analysis: 2026-11-21*
