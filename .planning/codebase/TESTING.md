# Testing Patterns

**Analysis Date:** 2026-11-21

## Test Framework

**Runner:**
- doctest (single header, vendored at `third_party/doctest.h`, C++11-compatible).
- Entry point: `tests/test_main.cpp` (`#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN` + `#include "doctest.h"`). Do not add another main.
- Config: `CMakeLists.txt` — `file(GLOB TEST_SOURCES CONFIGURE_DEPENDS tests/*.cpp)` → `unit_tests` executable, linked to the `reactor` static library, `third_party` included as `SYSTEM`, flags `-Wall -Wextra -Wshadow` (no float-only flags: references are computed in `double` on purpose). Registered with CTest via `add_test(NAME unit_tests COMMAND unit_tests)`.
- Directory is `tests/` (not `test/`) so PlatformIO does not try to build it for the board.

**Assertion Library:** doctest macros — `CHECK`, `CHECK_FALSE`, `REQUIRE` (only for preconditions like `REQUIRE(r.init());`), `doctest::Approx(x).epsilon(rel)`, `MESSAGE(...)`.

**Current status:** 23 test cases, 1892 assertions, all passing (`[doctest] Status: SUCCESS!`).

**Run Commands (PowerShell, Windows/MinGW):**
```powershell
$env:Path = [Environment]::GetEnvironmentVariable('Path','User') + ';' + [Environment]::GetEnvironmentVariable('Path','Machine')
cd "C:\Users\e1302902\Documents\ENGR217\Nuclear Reactor Simulator"
cmake --build build; .\build\unit_tests.exe      # build + run all
.\build\unit_tests.exe -ltc                      # list test case names
.\build\unit_tests.exe -tc="T12*"                # run one test by name pattern
.\build\unit_tests.exe -s                        # show successful assertions too
ctest --test-dir build                           # via CTest (single CTest entry)
```
- New `tests/*.cpp` files are picked up automatically (`CONFIGURE_DEPENDS`).
- No watch mode, no coverage tooling configured.

## Test File Organization

**Location:** Separate `tests/` directory; one file per library area, header comment lists which T-IDs it covers.

**Naming:** `tests/test_<area>.cpp`; test case names start with the T-ID then a plain-English expectation: `TEST_CASE("T1 critical reactor holds power for 60000 steps")`. Facade tests without a spec ID use a sentence (`"Reactor rejects a positive temperature coefficient"`).

**Structure:**
```
tests/
├── test_main.cpp      # doctest main only
├── test_kinetics.cpp  # T1–T8, T14 (solver part), T16
├── test_plant.cpp     # T9–T12, T17, T18
└── test_reactor.cpp   # T13, T14 (facade part), T15 + 5 facade cases
```

## Test-ID Mapping (docs/CODE-GUIDE.html §8 → tests)

| ID | Spec expectation (CODE-GUIDE) | Test location | Tolerance used |
|----|-------------------------------|---------------|----------------|
| T1 | Critical hold 60 000 steps, \|n−1\| < 1e-4 (V1) | `tests/test_kinetics.cpp` "T1 critical reactor holds power…" | abs `< 1e-4` |
| T2 | Prompt jump +$0.31, n ≈ 1.44 after ~0.1 s (V3) | `test_kinetics.cpp` "T2 prompt jump…" | `Approx(1.45).epsilon(0.02)`, h = 1 ms |
| T3 | +100 pcm period = inhour root (55–57 s) within 2 % (V2) | `test_kinetics.cpp` "T3 …" | root in (55, 57); measured `epsilon(0.02)` |
| T4 | +$0.1 / +$0.5 periods within 2 % of inhour (V3) | `test_kinetics.cpp` "T4 …" | roots ≈98 s (2 %), ≈5.8 s (3 %); measured 2 % |
| T5 | SCRAM −$5: drop to 1/6, late period ≈ −80 s (V4) | `test_kinetics.cpp` "T5 …" | drop `epsilon(0.05)`; root −80.6 (5 %); measured 2 % |
| T6 | Explicit Euler +$0.1: 40 ms diverges, 20 ms bounded (V7) | `test_kinetics.cpp` "T6 …" | divergence = \|n\| ≥ 1e6 or n < 0; implicit 1 < n < 2; 20 ms ≈ 1.2 (10 %) |
| T7 | Halving h halves error (V6) | `test_kinetics.cpp` "T7 …" | error ratio `Approx(2.0).epsilon(0.2)`, reference h = 0.5 ms |
| T8 | Subcritical + source → −SΛ/ρ (F13) | `test_kinetics.cpp` "T8 …" | 1 % |
| T9 | Rod worth 0, ρw/2, ρw; critical at 0.5 | `tests/test_plant.cpp` "T9 …" | default Approx; also checks −$1.1 rod, −$5.1 rod+safety |
| T10 | Rod moves ≤ v·h per step | `test_plant.cpp` "T10 …" | `<= v*h*1.0001f`, bounds [0,1] incl. demands 2.0, −1.0 |
| T11 | T → Tc + P/K | `test_plant.cpp` "T11 …" | `epsilon(1e-4)`; also `isValid` rejects αT > 0 |
| T12 | +$0.2 pull, 1800 s: ΔT ≈ 26 K, n ≈ 1.87, ρ within 1 pcm, ringing dies (V8) | `test_plant.cpp` "T12 …" | 2 %; \|ρ\| < 1e-5; late spread < 1 %; overshoot logged via `MESSAGE` (≈81 %) |
| T13 | Trip latch; reset only with rods in | `tests/test_reactor.cpp` "T13 …" | exact bitmask checks |
| T14 | Prompt critical: trips, no NaN, denominator > 0 | `test_kinetics.cpp` "T14 prompt critical: substeps…" (solver) and `test_reactor.cpp` "T14 prompt critical trips…" (facade) | `isfinite`, `n == Approx(N_MAX)` |
| T15 | Period meter recovers τ within 1 % | `test_reactor.cpp` "T15 …" | `epsilon(0.01)` for τ = 20, −50 s; steady → `PERIOD_INF`; `sur`, `logPower` |
| T16 | −$2 → −$1 doubles source level | `test_kinetics.cpp` "T16 …" | level 0.1 %, ratio 1 % |
| T17 | S-curve vs DOE Example 3 (260 pcm / 16 in) within 3 % of total | `test_plant.cpp` "T17 …" | abs `<= 0.03*260`, midpoint `Approx(130)` |
| T18 | Overcooling −10 K: T → Tref, P → 1.33 | `test_plant.cpp` "T18 …" | T `epsilon(2e-3)`, n 2 % |

Extra (no T-ID), all in `tests/test_reactor.cpp`: "Reactor holds full power at the critical rod position" (600 s, n within 1e-3, no trips/warnings, `t` accumulates to 600), "Reactor rejects a positive temperature coefficient", "Override is a test mode: power and period trips are bypassed", "Fast rod withdrawal trips on high power, not on a fault".

Doctest runs files in link order, so `-ltc` lists T14/T16 before T9 — that is expected.

## Test Structure

**Suite Organization:** flat `TEST_CASE`s, no `SUBCASE`s or fixtures. File-scope `static const` parameter sets and `static` helpers at the top:
```cpp
// test_kinetics.cpp - point kinetics solver tests (T1-T8, T14 solver part, T16).
// Units: time s, n normalized power, rho dk/k.
#include <math.h>

#include "doctest.h"
#include "kinetics.h"

static const KineticsParams P = keepinU235();

TEST_CASE("T1 critical reactor holds power for 60000 steps") {
  Kinetics k;
  initEquilibrium(k, P, 1.0f);
  run(k, 0.0f, 0.0f, 0.01f, 60000);
  CHECK(fabs(k.n - 1.0) < 1e-4);
}
```

**Patterns:**
- Setup: always start from a known equilibrium (`initEquilibrium(k, P, n0)` or `REQUIRE(r.init())`), then march with a fixed `h` for a fixed number of steps (`static_cast<int>(seconds / h + 0.5)`).
- Teardown: none (stack objects).
- Assertions: compare against an analytic or independently computed reference, never against a recorded output from the same code. Put the formula/expected value in a comment (`// F11: beta / (beta - rho) = 1.449`).
- Multi-phase scenarios (T13) drive the facade step by step, mutating one `Inputs` field at a time and checking the trip word after each phase.

## Reference Helpers

Defined `static` in each test file (single-use → kept local):
- `run(Kinetics&, rho, S, h, steps)` — repeated `stepImplicit` (`test_kinetics.cpp`).
- `inhourOmega(double rho)` — six-group inhour root (F9) by 200-iteration bisection **in double**; the ground truth for T3–T5 (`test_kinetics.cpp`).
- `measuredPeriod(rho, h, t1, t2)` — asymptotic period `(t2-t1)/ln(n2/n1)` from a step from critical (`test_kinetics.cpp`).
- `runPlant(rodTarget, Tcool, seconds)` → `PlantRun{n, T, rho, maxN, lateSpread}` — kinetics + rod + heat + feedback loop without protection; tracks overshoot and late-time ringing (`test_plant.cpp`).
- `rodFor(float rho)` — bisection for the rod position giving a target worth (`test_plant.cpp`; intentional duplicate in `desktop/sim_cli.cpp`, marked `// note:`).
- `runFor(Reactor&, const Inputs&, seconds)` — steps the facade at 10 ms, returns the last `Readings&` (`test_reactor.cpp`).
- External reference: `tools/reference.py` (SciPy `solve_ivp` + `brentq` inhour) and `tools/compare.py` overlay CLI CSVs against it (figures in `figures/`). These are manual validation, not part of `unit_tests`.

## Tolerances

- Use `doctest::Approx(x).epsilon(rel)` (relative) for physics quantities; plain `fabs(a - b) < tol` for absolute checks near zero (T1, T9 critical worth, T12 ρ).
- Typical: 2 % for period/level comparisons against inhour or steady-state formulas; 1 % for meter and source-level checks; 1e-4 for conservation-type checks (T1, T11); 5 % where the analytic value is itself approximate (T5 prompt drop, −80 s).
- Bounds rather than equality for qualitative behavior (T6 divergence, T12 overshoot > 0, override `n > 2`).
- Exact `==` only for integers/bitmasks (`trips == TRIP_MANUAL`) and assigned sentinels (`period == PERIOD_INF`).
- Literal dollars are written as `x * 0.0065f` in tests (not `DOLLAR`); `BETA = 0.0065` as a `double` in `test_kinetics.cpp`.

## Mocking

**Framework:** None. The library is pure (no clock, no I/O), so tests call it directly with chosen `h`, `rho`, `Inputs`. Feed synthetic signals instead of mocks (e.g. T15 feeds `expf(t / tau)` into `update()`).

**What to Mock:** Nothing in `lib/reactor/src/`.

**What NOT to Mock:** Do not stub `stepImplicit` or plant functions in facade tests; T13/T14 rely on real dynamics.

## Fixtures and Factories

```cpp
static const KineticsParams P = keepinU235();   // test_kinetics.cpp
static const PlantParams PP = defaultPlant();   // test_plant.cpp
PlantParams bad = PP; bad.alphaT = 1.0e-5f;     // copy-and-tweak for negative cases
```
- Factories live in `lib/reactor/src/params.h`. No fixture files; no data files read by tests.

## Coverage

**Requirements:** None enforced; no gcov/lcov target. Coverage is tracked by T-ID against `docs/CODE-GUIDE.html` §8 — all T1–T18 are present.

## Test Types

**Unit Tests:** `tests/*.cpp` — library functions and the `Reactor` facade, on desktop only.

**Integration / validation:** Manual. `desktop/sim_cli.cpp` scenarios (`hold, step, ramp, scram, rodpull, startup`) → CSV → `tools/compare.py` vs `tools/reference.py` → `figures/*.png`. Hardware runs captured with `tools/capture_serial.py` into `results/*.csv` (gitignored). On-board timing: serial command `b` runs 1000 `stepImplicit` calls and prints µs/step.

**E2E Tests:** Not used.

## Coverage Gaps vs Spec

- **`TRIP_FAULT` path never triggered**: no test feeds NaN/inf to `evaluate()` or forces `stepImplicit` to return `false`; tests only assert the fault bit is *absent* (`test_reactor.cpp` T14, fast withdrawal). Priority: High (safety logic).
- **`TRIP_HIGH_TEMP`, `WARN_TEMP`, `WARN_PERIOD`** have no direct test; only `WARN_POWER` and `TRIP_HIGH_POWER|TRIP_SHORT_PERIOD` (combined) are checked. Add direct `evaluate()`/`warnings()` threshold tests in `tests/test_reactor.cpp`. Priority: Medium.
- **Period trip boundary** (`0 < period < periodTrip`, negative periods must not trip) and **`TRIP_PROMPT_CRIT` at exactly $1.00** untested. Priority: Medium.
- **`Reactor::init` failure keeps old state** (documented in `reactor.h`) not verified — test only checks the `false` return. Priority: Low.
- **Long-run time accumulation** (`tWhole_`/`tFrac_`, the >1 h float-precision fix) only checked to 600 s. Priority: Low.
- **Spec drift, not gaps:** CODE-GUIDE T2 says n ≈ 1.44; test uses 1.45 ± 2 %. CODE-GUIDE signatures show `void stepImplicit` and `stepTemperature(float T, …)`; code uses `bool` and `dT`. Tests follow the code.
- **Not unit-tested at all:** `desktop/sim_cli.cpp` (argument parsing, scenarios), firmware `src/` (`main.cpp` accumulator/`MAX_STEPS`/`ACC_CAP_MS`, serial command parser, `panel_io.cpp` debounce/pot mapping, `display.cpp`), and `tools/*.py`. Firmware code depends on `<Arduino.h>` and cannot build on desktop as-is.
- **No CI**: tests run only when invoked locally.

## Common Patterns

**Long-time integration:**
```cpp
run(k, rho, S, 0.1f, 30000);  // 3000 s
CHECK(k.n == doctest::Approx(-S * P.Lambda / rho).epsilon(0.01));
```

**Error / negative testing:**
```cpp
PlantParams bad = defaultPlant();
bad.alphaT = 1.0e-5f;
Reactor r;
CHECK_FALSE(r.init(bad));
```

**Bitmask checks:**
```cpp
CHECK((hit.trips & TRIP_PROMPT_CRIT) != 0);
CHECK((hit.trips & TRIP_FAULT) == 0);
```

**Recording a measured value:** `MESSAGE("T12 overshoot " << overshoot * 100.0 << " %");`

---

*Testing analysis: 2026-11-21*
