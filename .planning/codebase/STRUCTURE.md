# Codebase Structure

**Analysis Date:** 2026-11-21

## Directory Layout

```
Nuclear Reactor Simulator/
├── lib/reactor/src/   # Shared portable physics library (firmware + desktop)
├── src/               # Arduino UNO R4 Minima firmware (PlatformIO)
├── desktop/           # Desktop CLI runner (CMake)
├── tests/             # doctest unit tests (CMake)
├── third_party/       # Vendored doctest.h
├── tools/             # Python reference model, comparison, serial capture
├── docs/              # PLAN.md/.html, CODE-GUIDE.html, LOG.md
├── figures/           # Generated plots from tools/compare.py
├── results/           # Captured / generated CSV runs
├── CMakeLists.txt     # Desktop build: reactor lib, sim_cli, unit_tests
├── platformio.ini     # Firmware build env uno_r4_minima
└── README.md, LICENSE, .gitignore
```

## Directory Purposes

**`lib/reactor/src/`:**
- Purpose: physics core; auto-discovered by PlatformIO, globbed by CMake.
- Key files: `params.h`, `kinetics.{h,cpp}`, `plant.{h,cpp}`, `instruments.{h,cpp}`, `protection.{h,cpp}`, `reactor.{h,cpp}`.

**`src/`:**
- Purpose: hardware shell. `main.cpp` (loop, serial), `panel_io.{h,cpp}` (pins, pots, buttons, LEDs, buzzer, servo), `display.{h,cpp}` (SSD1306).

**`desktop/`:** `sim_cli.cpp` — scenario runner, CSV to stdout.

**`tests/`:** `test_main.cpp` (doctest main), `test_kinetics.cpp`, `test_plant.cpp`, `test_reactor.cpp`.

**`tools/`:** `reference.py` (SciPy model), `compare.py` (overlays/convergence/stability → `figures/`), `capture_serial.py` (serial → CSV), `requirements.txt`.

**`docs/`:** `PLAN.md` (design + formula numbers F*), `CODE-GUIDE.html`, `PLAN.html`, `LOG.md`.

## Key File Locations

**Entry Points:**
- `src/main.cpp`: firmware `setup()`/`loop()`
- `desktop/sim_cli.cpp`: desktop `main()`
- `tests/test_main.cpp`: test runner
- `tools/compare.py`: validation pipeline

**Configuration:**
- `platformio.ini`: board, lib_deps (Adafruit SSD1306, GFX, Servo), warning flags
- `CMakeLists.txt`: C++11, warnings, static MinGW link, targets
- `lib/reactor/src/params.h`: all tunable physics constants and setpoints
- `src/panel_io.cpp`: pin map and pot ranges (top-of-file `static const`)
- `src/main.cpp`: timing constants (`DT_MS`, `ACC_CAP_MS`, `PRINT_MS`, `DRAW_MS`)

**Core Logic:** `lib/reactor/src/reactor.cpp` (step order and latch)

**Testing:** `tests/`, `third_party/doctest.h`

## Naming Conventions

**Files:** lowercase snake_case, `.h`/`.cpp` pairs (`panel_io.cpp`); tests `test_<module>.cpp`; Python snake_case.
**Types:** PascalCase structs/classes (`Reactor`, `PeriodMeter`, `PlantParams`).
**Functions:** camelCase free functions (`stepImplicit`, `moveRod`, `tryReset`).
**Members:** trailing underscore for private class members (`latched_`, `rodPot_`); POD struct fields have none.
**Constants:** `UPPER_SNAKE` `const`/`static const` (`TRIP_HIGH_POWER`, `DT_MS`, `PIN_SCRAM`); physics symbols keep math names (`Lambda`, `rhoW`, `alphaT`, `MC`).
**Header guards:** `REACTOR_<NAME>_H` in the library, `<NAME>_H` in `src/`.
**File headers:** first line `// <file> - <purpose>.`, second line `// Units: ...`.

## Where to Add New Code

**New physics behavior:** new or existing module in `lib/reactor/src/` (free functions + POD state, `float` only, no Arduino headers); wire it into `Reactor::step` in `reactor.cpp`; parameters in `params.h`.
- Tests: `tests/test_<module>.cpp` (picked up by the CMake glob).

**New trip/warning:** bit constant in `protection.h`, condition in `evaluate()`/`warnings()`, setpoint in `Setpoints`/`defaultSetpoints()`; decide whether it joins `OVERRIDE_BYPASS` in `reactor.cpp`.

**New panel hardware:** pin constant and logic in `src/panel_io.cpp`; new input fields go in `Inputs` (`reactor.h`).

**New serial command / output format:** `handleCommand()` / `printReadings()` in `src/main.cpp`.

**New OLED content:** `src/display.cpp`.

**New desktop scenario:** branch in `desktop/sim_cli.cpp`; Python counterpart in `tools/reference.py` / `tools/compare.py` if validated.

**Utilities:** no shared util dir; keep helpers `static` in the file that uses them.

## Special Directories

**`.pio/`:** PlatformIO build output — Generated: Yes, Committed: No.
**`build/`:** CMake build output (`sim_cli`, `unit_tests`) — Generated: Yes, Committed: No.
**`.venv/`:** Python virtualenv — Generated: Yes, Committed: No.
**`figures/`:** plots from `tools/compare.py` — Generated: Yes, Committed: Yes.
**`results/`:** CSV runs (`rodpull.csv`, `scram.csv`, `startup.csv`) — Generated: Yes, Committed: Yes.
**`third_party/`:** vendored `doctest.h` — Generated: No, Committed: Yes.
**`.planning/`:** GSD planning artifacts.

---

*Structure analysis: 2026-11-21*
