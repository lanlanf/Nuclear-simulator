---
phase: docs-vs-code
reviewed: 2026-11-20T00:00:00Z
depth: deep
files_reviewed: 29
files_reviewed_list:
  - README.md
  - docs/PLAN.md
  - docs/PLAN.html
  - docs/CODE-GUIDE.html
  - docs/LOG.md
  - lib/reactor/src/params.h
  - lib/reactor/src/kinetics.h
  - lib/reactor/src/kinetics.cpp
  - lib/reactor/src/plant.h
  - lib/reactor/src/plant.cpp
  - lib/reactor/src/instruments.h
  - lib/reactor/src/instruments.cpp
  - lib/reactor/src/protection.h
  - lib/reactor/src/protection.cpp
  - lib/reactor/src/reactor.h
  - lib/reactor/src/reactor.cpp
  - src/main.cpp
  - src/panel_io.h
  - src/panel_io.cpp
  - src/display.cpp
  - desktop/sim_cli.cpp
  - tests/test_kinetics.cpp
  - tests/test_plant.cpp
  - tests/test_reactor.cpp
  - tools/reference.py
  - tools/compare.py
  - tools/capture_serial.py
  - CMakeLists.txt
  - platformio.ini
findings:
  critical: 0
  warning: 11
  info: 15
  total: 26
status: issues_found
---

# Documentation vs Code: Review Report

**Reviewed:** 2026-11-20
**Depth:** deep (every number, pin, command, setpoint, signature, test ID, CSV column and build command in the docs was checked against the code)
**Status:** issues_found

## Summary

- **Wiring, pins, setpoints and trip bits agree everywhere.** README, PLAN.md §4.3, PLAN.html §8.2 and `panel_io.cpp:7-16` all give D2 SCRAM, D3 reset, D4 buzzer, D5/D6/D7/D12 LEDs, D9 servo, A0/A1 pots and I²C 0x3C. The README setpoints (120 %, 330 K, 10 s; warnings at 105 %, 325 K, 30 s) match `params.h:66`. The trip bits match `protection.h:10-15`. The serial command set matches `main.cpp:53-75`. No doc instruction would make the user wire or run something wrong, so there are **no Critical findings**.
- **The two corrections are complete.**
  - The explicit-Euler limit reads ≈31 ms at ρ = 0 and ~5 ms after a −$5 SCRAM in PLAN.md:119, PLAN.md:245 (V7), PLAN.html:192-193 (§3), PLAN.html:319 and PLAN.html:493.
  - CODE-GUIDE F8 states only the formula, which is correct. T6 (CODE-GUIDE T6 row) says 34 ms at +$0.1, which matches `test_kinetics.cpp:83` and the `compare.py:96` plot title.
  - The overshoot reads ~80 % in PLAN.md:98/246, PLAN.html:289/494 and CODE-GUIDE:437.
  - The only "3 ms" and "10–20 %" left are in LOG.md:31/35, where they describe the old error on purpose. That is correct.
- **The main problem is CODE-GUIDE drift.** Its API section (§6), OLED layout (§7) and config snippets (§10) still describe the pre-implementation design. LOG.md explains only 6 of the 10 deliberate deviations.
- The LOG validation numbers (error table, slope 1.02, RAM/flash %) were **not re-run**. That would need a build and a `compare.py` run, which this read-only review does not do. The startup prediction n = 0.619 and T = 308.6 K was checked by hand against `rodWorth(0.48)` and the feedback equation, and it is consistent.

## Warnings

### DOC-01 (Warning): LOG.md doesn't explain 4 of the 10 listed deliberate deviations, and only implies a fifth
- **Doc:** docs/LOG.md:28-54 (Findings and Design choices)
- **Code:** reactor.h:15 (`Inputs::Tcool`), reactor.h:33 (`bool init`), kinetics.h:22 (`bool stepImplicit`), main.cpp:89 and sim_cli.cpp:23 (CSV `rho` = `rhoDollars`), reactor.h:46-47 (`tWhole_` + `tFrac_`)
- **Doc says:** it explains dT, ω smoothing, the reset pot rule, the override bypass and the 5-line OLED. It mentions only the coolant-pot *range*.
- **Code does:** four more deviations have no rationale anywhere in the docs:
  - `Inputs` gained a `Tcool` field.
  - `Reactor::init` returns `bool` and rejects αT > 0.
  - `stepImplicit` returns `bool` (fault on a non-positive denominator or non-finite n).
  - Time is kept as `uint32` whole seconds plus a `float` fraction, because a float loses 10 ms steps after about an hour (reactor.h:46 comment).

  The CSV `rho` column in dollars is implied by README:56 but never called a change from "absolute Δk/k" (CODE-GUIDE §2 step 6).
- **Change:** docs. Add one "Design choices" bullet to LOG.md for each of these.

### DOC-02 (Warning): CODE-GUIDE `stepTemperature`/`feedback` signatures and code snippet take T, but the code takes dT
- **Doc:** docs/CODE-GUIDE.html:301-306
- **Code:** plant.h:27/30, plant.cpp:25-29
- **Doc says:** `stepTemperature(float T, …)` returns `(T + h/MC·(P + K·Tcool)) / (1 + hK/MC)`, and `feedback(float T, …)`.
- **Code does:** both functions take `dT = T − Tref`. The update is `(dT + h/MC·(P − K(Tref−Tcool))) / (1 + hK/MC)`. Passing absolute T to the real functions gives a reading about 320 K off.
- **Change:** docs. Update the signatures and the snippet, and note the float-stagnation reason (already in LOG.md:39-41).

### DOC-03 (Warning): CODE-GUIDE `stepImplicit`, `Reactor::init` and `Inputs` don't match the code
- **Doc:** docs/CODE-GUIDE.html:291, 324, 329
- **Code:** kinetics.h:22, reactor.h:13-20, reactor.h:33
- **Doc says:**
  - `void stepImplicit(...)` "asserts the denominator is > 0".
  - `struct Inputs { rodDemand, scram, reset, overrideOn, rhoOverride }`.
  - `void init();`
- **Code does:**
  - `bool stepImplicit` returns false instead of asserting. reactor.cpp:63 turns that into `TRIP_FAULT`.
  - `Inputs` also has `float Tcool` (default 290) and default member initializers.
  - `bool init(const PlantParams& = defaultPlant(), const Setpoints& = defaultSetpoints())`.
- **Change:** docs.

### DOC-04 (Warning): CODE-GUIDE protection API and reset rule are stale
- **Doc:** docs/CODE-GUIDE.html:318-319; T13 row, line 438
- **Code:** protection.h:22-28, reactor.cpp:33-36, test_reactor.cpp:51 and 68-73
- **Doc says:** `evaluate(readings, setpoints)` and `tryReset(latched, rodPos)`. Reset needs "rods fully in". T13 expects "reset only with rods in".
- **Code does:** `evaluate(n, T, period, rhoDollars, s)`, a separate `warnings(...)`, and `tryReset(latched, rodPos, present)`. Reactor passes `max(rod_, in.rodDemand)`, so the **rod pot must also be in**. T13's actual name is "reset only with rods and rod demand in". Someone reading the guide would press reset with the pot up and see it silently refused. README:121-123 has it right.
- **Change:** docs.

### DOC-05 (Warning): Period-meter description (CODE-GUIDE, PLAN.md, PLAN.html) contradicts the code
- **Doc:** docs/CODE-GUIDE.html:310-311; docs/PLAN.md:159; docs/PLAN.html:344
- **Code:** instruments.h:17-18 and 23-24, instruments.cpp:22-38
- **Doc says:**
  - CODE-GUIDE: "EMA-smoothed period", reported as ∞ "when |Δln n| is tiny".
  - PLAN: `T = Δt/ln(n_k/n_{k-1})` (a per-step ratio), smoothed with an EMA.
- **Code does:** a 1 s windowed slope with EMA (weight 0.2) on **ω = 1/τ**. It reports `PERIOD_INF = 1e9` when |ω| < 1e-4. CODE-GUIDE §5.1 itself rejects the per-step ratio PLAN still shows. LOG.md:42-43 records the change.
- **Change:** docs (all three).

### DOC-06 (Warning): Short-period trip is documented as |T| < 10 s, but the code trips only on positive periods
- **Doc:** docs/PLAN.md:159 and 226; docs/PLAN.html:344 and 476
- **Code:** protection.cpp:12
- **Doc says:** the trip fires at `|T| < 10 s` / `|period| < 10 s`.
- **Code does:** `period > 0 && period < periodTrip`. Fast negative periods, such as after a SCRAM or a rod insertion, do not trip. README:118 ("short period (< 10 s)") and CODE-GUIDE are consistent with the code.
- **Change:** docs. Write `0 < τ < 10 s`.

### DOC-07 (Warning): The CODE-GUIDE OLED layout is 4 lines; the firmware draws 5 with different content
- **Doc:** docs/CODE-GUIDE.html:389-392
- **Code:** display.cpp:36-73
- **Doc says:**
  - Line 3 is `T 321.4K  per 112s  SUR 0.23`.
  - Line 4 is the inverted state/trip text.
- **Code does:**
  - Line 3 is `T …K per …s` (y = 24).
  - Line 4 is `SUR … DPM` (y = 36).
  - Line 5 is the state text, inverted in a box at y = 50-63.
  - There is also a `TEST: RHO OVERRIDE` state.

  LOG.md:51-52 explains this, but the spec was not updated.
- **Change:** docs.

### DOC-08 (Warning): Serial-output formats in all three plan/guide docs don't match the firmware
- **Doc:**
  - docs/PLAN.md:228 (`P:..,rho$:..,T:..,SUR:..`) and PLAN.md:233 (CSV `t,n,rho,T`).
  - docs/PLAN.html:478 (`P:..,rho:..,T:..,SUR:..`).
  - docs/CODE-GUIDE.html:399 (plotter `P:1.02,T:321`, Teleplot `>P:1.02`).
- **Code:** main.cpp:56 and 86-127
- **Code does:**
  - Plotter mode prints `P:<percent>,rod:<percent>,rho_cents:<¢>,T_minus_300:<K>`, with no SUR.
  - Teleplot mode prints `>P` in **percent** (102.0, not 1.02), plus `>logP`, `>rho` ($), `>T`, `>rod` (%) and `>SUR`.
  - CSV mode prints `t,rho,n,T,period,sur,rod,trip`.

  PLAN.md's `t,n,rho,T` order swaps n and rho, which would mislabel columns for anyone parsing to the plan.
- **Change:** docs. README and CODE-GUIDE §2 step 7 already give the right CSV header.

### DOC-09 (Warning): PLAN.md and PLAN.html disagree on solver cost
- **Doc:** docs/PLAN.md:121 ("about 40 floating-point operations") vs docs/PLAN.html:310 and 384 ("about 60 flops", "~18 divides"); CODE-GUIDE.html:232 also says 60.
- **Code:** kinetics.cpp:27-40. Per step there are 6 × (1 add + 1 mul to form d, 4 mul/div + 2 add for num/sumLB) plus the denominator and the 6-term C update. That is roughly 60+ operations, so 60 is closer.
- **Change:** docs. Make PLAN.md say "about 60".

### DOC-10 (Warning): PLAN.html §9.1 folder layout lists files that don't exist
- **Doc:** docs/PLAN.html:455-459
- **Code:** repo tree
- **Doc says:** `reactor.h/.cpp` at the root, `desktop/main.cpp`, `reference/pke_scipy.py` and `arduino/reactor_panel.ino`. It also says `step(rodFraction, dt)`.
- **Code does:** the library is in `lib/reactor/src/` (6 modules), along with `desktop/sim_cli.cpp`, `tools/reference.py` and the firmware in `src/main.cpp` and others. `Reactor::step(const Inputs&, float h)`. PLAN.md has no such section, so the two plans also disagree here.
- **Change:** docs. Point PLAN.html §9.1 to the CODE-GUIDE §4 layout.

### DOC-11 (Warning): CODE-GUIDE `platformio.ini` snippet differs from the real file
- **Doc:** docs/CODE-GUIDE.html:483-495
- **Code:** platformio.ini:1-14
- **Doc says:** a `[platformio] default_envs` section, `lib_deps` entry `Servo` (flagged UNVERIFIED), and `build_flags = -Wall …`.
- **Code does:** there is no `[platformio]` section. The file uses `arduino-libraries/Servo` and **`build_src_flags`**, which apply the warnings to project sources only (comment at platformio.ini:13: "the Arduino core is not warning-clean"). If someone copies the guide's `build_flags`, the core build fills with warnings and the "zero warnings" rule (CODE-GUIDE §11) can't be met.
- **Change:** docs. Copy the real file in, and resolve the Servo UNVERIFIED note (the full name is what works).

## Info

### DOC-12 (Info): README says a C++17 compiler is needed; the project builds as C++11
- **Doc:** README.md:30. **Code:** CMakeLists.txt:4 (`CMAKE_CXX_STANDARD 11`); CODE-GUIDE.html §1 says C++11.
- **Change:** docs. Say "C++11".

### DOC-13 (Info): `ctest --test-dir` needs CMake ≥ 3.20, but the README says ≥ 3.16
- **Doc:** README.md:30 and 35. **Code:** CMakeLists.txt:1 (`VERSION 3.16`).
- **Change:** docs. Either say "CMake ≥ 3.20 for `--test-dir`" or use `cd build; ctest`.

### DOC-14 (Info): The README buzzer note says "until reset"; the code silences it when reset is pressed
- **Doc:** README.md:89. **Code:** main.cpp:156 (`if (in.reset) panel.acknowledge()`), panel_io.cpp:71-77.
- **Code does:** a reset press acknowledges (silences) the buzzer even when the reset is refused and the trip stays latched. A new trip bit re-arms it.
- **Change:** docs. Write "until reset is pressed (acknowledge)".

### DOC-15 (Info): Test IDs: README says T1–T18, but there are 23 cases and T14 is used twice
- **Doc:** README.md:21; CODE-GUIDE.html:426-444. **Code:**
  - T14 is defined at test_kinetics.cpp:131 and again at test_reactor.cpp:82.
  - Five cases have no ID: test_reactor.cpp:32, 44, 98 and 114, plus T14's second half.
- LOG.md:5/8 correctly says 23 tests.
- **Change:** docs. Mention the extra facade/override cases, or give them IDs (T19+) in both the code and the guide.

### DOC-16 (Info): CODE-GUIDE line budgets are exceeded by some files
- **Doc:** docs/CODE-GUIDE.html:170-185.
- **Code (actual line counts):**

  | File | Actual | Budget |
  |---|---|---|
  | `sim_cli.cpp` | 169 | 120 |
  | `tests/*.cpp` | 353 | 300 |
  | `main.cpp` | 161 | 150 |
  | `reactor.h/.cpp` | 115 | 100 |

  Every other file is under budget: params 62/60, kinetics 65/100, plant 45/70, instruments 57/60, protection 47/60, panel_io 102/120, display 76/80, reference 103/120, compare 101/150, capture 36/50.
- **Change:** docs. Update the table, or leave it and note that the figures are estimates.

### DOC-17 (Info): CODE-GUIDE `step()` order doesn't match reactor.cpp
- **Doc:** docs/CODE-GUIDE.html:333-340 ("Run evaluate and latch", then "Fill in Readings"; no reset step).
- **Code:** reactor.cpp:32-36 handle SCRAM/reset first. reactor.cpp:67-76 fill Readings *before* `evaluate` (which reads `r_.n`, `r_.T`, `r_.period`), and then apply the override mask.
- **Change:** docs.

### DOC-18 (Info): CODE-GUIDE §6 omits functions that exist
- **Doc:** docs/CODE-GUIDE.html:298-320. **Code:** `sCurve`, `safetyWorth` and `isValid` (plant.h:11, 17, 33); `warnings` (protection.h:25); `N_MAX` (kinetics.h:8); `PERIOD_INF`/`PERIOD_SAMPLES` (instruments.h:8-9).
- **Change:** docs.

### DOC-19 (Info): CODE-GUIDE CMakeLists snippet differs from the real file
- **Doc:** docs/CODE-GUIDE.html:499-515 (a global `add_compile_options` with `-Wfloat-conversion`; no `SYSTEM`; no MinGW static link).
- **Code:** CMakeLists.txt:9-31 (per-target warnings; tests drop the float flags on purpose; doctest is `SYSTEM`; `-static` on MinGW).
- **Change:** docs.

### DOC-20 (Info): CODE-GUIDE Python snippet doesn't match reference.py
- **Doc:** docs/CODE-GUIDE.html:466-476 (`rhs(t, y, rho_of_t, p)` without temperature; `inhour_root(rho, p)` solved in T over [1e-6, 1e7], positive ρ only).
- **Code:** reference.py:39-48 (`inhour_root(rho)` solved in ω, handles ρ < 0), reference.py:72-79 (8-state rhs with dT).
- **Change:** docs.

### DOC-21 (Info): β total is given as 0.0065, but the code computes 0.006502
- **Doc:** PLAN.md:31 and 55, PLAN.md:139 (`BETA = 0.0065` in the snippet), PLAN.html:249/261/329, CODE-GUIDE F4.
- **Code:** `totalBeta()` sums Keepin β_i to 0.006502 (kinetics.cpp:7-11). `DOLLAR = 0.0065` is used only for parameter scaling (params.h:8), and `r <dollars>` uses `totalBeta` (main.cpp:61). README:9 has 0.006502.
- **Change:** docs. Add a footnote saying the dollar conversion uses Σβ_i = 0.006502.

### DOC-22 (Info): PLAN says rho is "forced to −$5 if tripped"; the code drives both banks in at scram speed
- **Doc:** PLAN.md:222; PLAN.html:472.
- **Code:** reactor.cpp:39-41 (`moveRod(..., scramSpeed = 1/s)`, so ρ ramps to −$5.1 over ≤ 1 s, plus feedback).
- **Change:** docs.

### DOC-23 (Info): PLAN substep, LED and state-machine statements are not implemented
- **Doc:**
  - PLAN.md:223 says "optionally 2–5 substeps".
  - PLAN.md:11 and PLAN.html:85 list LEDs for subcritical/critical/supercritical/warning/SCRAM.
  - PLAN.md:231 and PLAN.html:479 describe a SHUTDOWN→…→RESET state machine.
- **Code:**
  - The solver uses 10 substeps, only when ρ ≥ β (kinetics.cpp:22).
  - The LEDs are green = normal, yellow = warning, red = tripped and blue = critical (panel_io.cpp:76-79). The sub- and supercritical states appear only as OLED text (display.cpp:19-21).
  - There is no explicit state machine, only the latched trip word.
- **Change:** docs.

### DOC-24 (Info): PLAN.md and PLAN.html disagree on V-test details and citation numbering
- **Doc:**
  - PLAN.html:495 has V9 (timing). PLAN.md §6 has no V9, but CODE-GUIDE.html:402 references V9.
  - PLAN.md:243 V4 includes "≈ β/(β−ρ)"; PLAN.html:490 omits it.
  - Citations differ: Bickel is [R8] in md and [R13] in html; DOE Vol. 1 is [R7] vs [R12]; and PLAN.md has an unnumbered IAEA entry.
  - PLAN.md also lacks the PLAN.html §2–4 and §7 sections. That is acceptable for a condensed version, but it is not stated.
- **Change:** docs. Add V9 to PLAN.md and align the citation IDs.

### DOC-25 (Info): The coolant pot is described as "coolant flow" in the BOM; the code uses it as coolant temperature
- **Doc:** PLAN.md:180; PLAN.html:405. **Code:** panel_io.cpp:18-19 and 48 (Tcool = 280 + 20·pot K). README:85 and PLAN §2.4 say temperature.
- **Change:** docs.

### DOC-26 (Info): CODE-GUIDE §4 says `docs/`, `figures/` and `results/` are "(ignored by Git)"
- **Doc:** docs/CODE-GUIDE.html:183. **Code:** .gitignore ignores only `results/*.csv`; `docs/` and `figures/` are tracked.
- **Change:** docs. Write "`results/*.csv` ignored".

## Verified consistent (no finding)

- Constants: Λ = 1e-4; ρw = $2.2; ρs = $4; rod speed 2 %/s; scram speed 1/s; MC = 5; K = 1/30; Tcool = 290; Tref = 320; αT = −5 pcm/K; S = 3e-4 (params.h vs PLAN §2 and reference.py).
- Derived physics checks:
  - Rod end at −$1.1, both banks at −$5.1 (T9).
  - +$0.2 rod pull gives 26 K and n = 1.87 (T12).
  - Overcooling gives n = 1.33 (T18).
  - Startup goes critical at ≈ 45 % (hand check).
  - Inhour periods: 98 s, 5.8 s, −80 s and 55-57 s (T3-T5).
  - SUR = 26.06/τ.
  - Source level ≈ 9e-7 at −$5.
- Firmware timing: DT 10 ms, MAX_STEPS 10, accumulator cap 250 ms, print every 100 ms, OLED every 200 ms, pot EMA 0.1, debounce 30 ms, servo 180·x with a ≥1° deadband, blue LED at |ρ| < 5 pcm, serial timeout 2 s at 115200 baud.
- sim_cli: scenarios, flags, defaults (ramp $0.5, rodpull $0.2, others $0.1; t-end 300 s, startup 4000 s), CSV header and the `inv_m` column match README:41-56 and CODE-GUIDE §8.
- tools: `requirements.txt` = numpy, scipy, matplotlib, pyserial (CODE-GUIDE §9, README). Radau tolerances are rtol 1e-8 and atol 1e-12. `capture_serial.py` sends `m c`.
- README build and flash commands (`cmake -G Ninja`, `pio run -t upload`, `pio device monitor`) and the README test-mode/protection text all match the code.

---

_Reviewed: 2026-11-20_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: deep_
