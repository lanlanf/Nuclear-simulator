# Nuclear Reactor Simulator (ENGR 217)

## What This Is

This is a solo ENGR 217 project, approved by the instructor: a real-time nuclear reactor control simulator built on an Arduino UNO R4 Minima.

- **Physics:** a six-group point-kinetics reactor with temperature feedback, control rods and a safety system runs live on the board.
- **Panel:** pots, buttons, LEDs, a buzzer, a rod-position servo and an OLED let you "operate" the reactor and trip it safely.
- **Desktop:** the same physics library builds on a PC for unit tests and for validation against a SciPy reference.

## Core Value

A working Arduino build on real hardware that runs the simulator correctly. That is the course's main requirement. The operator moves the rod, watches the power respond, and a SCRAM always shuts the reactor down.

## Requirements

### Validated

- ✓ Six-group point kinetics, closed-form backward Euler at h = 10 ms, float-only. Matches SciPy within 1 % — existing
- ✓ Rod S-curve ($2.2) plus safety bank ($4), lumped thermal model with negative feedback (dT formulation) — existing
- ✓ Period meter, SUR and log power instrumentation — existing
- ✓ Latched protection: power, temperature, period, prompt-critical, fault and manual trips, plus warnings — existing
- ✓ Desktop CLI scenarios (hold, step, ramp, scram, rodpull, startup) with CSV output — existing
- ✓ 23 doctest unit tests, all passing — existing
- ✓ Python reference, comparison plots and serial capture tools — existing
- ✓ Firmware compiles for UNO R4 Minima (16 % RAM, 29 % flash, zero warnings in project code) — existing

### Active

- [ ] SCRAM can never be blocked or lost. Fix serial blocking (FW-01), reset-vs-SCRAM ordering and the held SCRAM button (PHY-01, FW-02).
- [ ] Bad inputs can't poison the state: NaN/Inf rod demand, coolant temperature and `r` override (PHY-02, PHY-03, FW-03).
- [ ] Retune so standard demos don't trip: a +$0.2 rod pull and a −10 K coolant change should stay below the 1.20 power trip.
- [ ] Tests that catch real regressions: feedback through `Reactor::step`, the fault trip, setpoints pinned, the reset refusal, scram speed (TST-01…TST-16).
- [ ] Parts bill of materials ordered from Amazon, plus a simple wiring diagram.
- [ ] Hardware bring-up on a breadboard: each panel element verified, step timing measured with `b`, OLED draw time measured.
- [ ] End-to-end demo on hardware: startup, power hold, rod pull, SCRAM, reset.
- [ ] Docs match the code (REVIEW-docs warnings) and LOG.md is updated.
- [ ] Nice to have: CLI argument validation, compare.py pass/fail tolerances, pinned dependency versions (REVIEW-tools).

### Out of Scope

- A two-node fuel/coolant thermal model — a later upgrade; the lumped model is enough for the course.
- Porting to UNO R3 or clones — the R4's FPU and 32 KB RAM are relied on.
- Firmware unit tests on target and a CI pipeline — too much for a 2-week solo timeline.
- A formal report or video — not required; the code and build are the graded item. Figures exist if needed.

## Context

- **Reviews (2026-11-21):** the deep reviews are in `.planning/reviews/REVIEW-{physics,firmware,tests,tools,docs}.md` and the codebase map is in `.planning/codebase/`.
  - Totals: 5 Critical, 41 Warning, 45 Info.
  - The physics math was verified correct.
  - Mutation testing showed 26 of 32 one-line mutations survived.
- **Not yet on a board:** the firmware has not run on hardware, and no parts have been bought yet.
- **Course:** the course projects come from two manual volumes, but the instructor approved this custom project. The only hard requirement is that it is a working Arduino build.
- **Network workaround:** PlatformIO downloads need `pip-system-certs` in `.venv` because the work network intercepts TLS.

## Constraints

- **Timeline:** due within 2 weeks (about 2026-12-04). Ordering parts is on the critical path.
- **Hardware:** the board is an Arduino UNO R4 Minima. All parts come from Amazon with 1–2 day shipping, roughly $40–60.
- **Team:** solo.
- **Tech stack:** C++11, float-only physics shared by the desktop and firmware builds. PlatformIO for the firmware; CMake, MinGW and doctest for the desktop; Python 3.12 for the tools.
- **Safety concept:** a SCRAM must always insert the rods within one loop iteration, whatever the serial or OLED state.

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| UNO R4 Minima, not R3 | FPU and 32 KB RAM for 6-group float kinetics | ✓ Good |
| Backward Euler, closed form | Unconditionally stable on a stiff system; about 60 flops per step | ✓ Good |
| Integrate dT = T − Tref | Float stagnation near 320 K rounds away small imbalances | ✓ Good |
| Reset needs the rod pot fully in | Stops the rod running back out after a reset | ✓ Good |
| Retune so demos don't trip | Instructor demo should show control, not repeated trips | — Pending |
| Safety-critical fixes and hardware bring-up first | Two-week deadline; the Core Value is a working board | — Pending |
| Buy all parts (no course kit) | No kit provided | — Pending |

## Evolution

This document evolves at phase transitions and milestone boundaries.

**After each phase transition** (via `/gsd-transition`):
1. Requirements invalidated? Move them to Out of Scope with the reason.
2. Requirements validated? Move them to Validated with the phase reference.
3. New requirements emerged? Add them to Active.
4. Decisions to log? Add them to Key Decisions.
5. Is "What This Is" still accurate? Update it if it has drifted.

**After each milestone** (via `/gsd-complete-milestone`):
1. Review all sections.
2. Core Value check: is it still the right priority?
3. Audit Out of Scope: are the reasons still valid?
4. Update Context with the current state.

---
*Last updated: 2026-11-21 after initialization*
