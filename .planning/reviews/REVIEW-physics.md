---
area: physics-library-numerics
reviewed: 2026-11-20T00:00:00Z
depth: deep
files_reviewed: 11
files_reviewed_list:
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
findings:
  critical: 1
  warning: 3
  info: 6
  total: 10
status: issues_found
---

# Physics Library: Code Review Report

**Depth:** deep (by-hand derivation; I did not run any experiments because the terminal returned stale output)
**Status:** issues_found

## Summary

The numerical core checks out against the spec. I re-derived these by hand:
- **Backward-Euler kinetics.** Eliminating C_i^{k+1} gives
  `n(1 - h(ρ-β)/Λ - h²/Λ·Σλβ/(1+hλ)) = n + hS + hΣλC/(1+hλ)`, which matches the code (kinetics.cpp:26-34).
- **Implicit heat balance.** The code (plant.cpp:25-27) matches the derivation.
- **S-curve rod worth.** It runs from -$1.1 to +$1.1, crosses zero at x = 0.5, and gives -$5.1 with both banks in.
- **SUR.** 60/(τ·ln10) = 26.06/τ.
- **Equilibrium precursors.** They are correct.

Positivity holds whenever `den > 0`, and `!(den > 0)` also catches a NaN ρ. The serious defects are in the **latch/reset logic** (Reactor::step) and in **input-NaN poisoning** of persistent state.

## Critical Issues

### PHY-01: Holding Reset defeats a held manual SCRAM: the latch clears every step and the rods withdraw

**File:** `lib/reactor/src/reactor.cpp:32-35` (and `protection.cpp:25-29`)
**Problem:** `in.scram` sets `TRIP_MANUAL` in `latched_`. The very next statement calls `tryReset`, and that check looks only at `present_`. `present_` comes from `evaluate()`, which never contains `TRIP_MANUAL`. The result:
- With scram and reset both asserted, and the rod and demand below 0.01, the latch clears on **every** step.
- Step 1 then runs the "not tripped" branch: the safety bank moves *out* at `rodSpeed`, and the regulating rod moves toward the demand.
- `r_.trips` reports 0 while the SCRAM button is physically held.

**Evidence (trace):** I traced one step with rod_ = 0, rodDemand = 0, scram = 1, reset = 1:
1. `latched_ = MANUAL`.
2. `tryReset(…, 0, present_ = 0)` returns true, so `latched_ = 0`.
3. `safety_ += 0.02·h`.
4. `present` has no MANUAL bit, so `latched_` stays 0.

After 50 s the safety bank is fully withdrawn (+$4) with SCRAM held. The core is still at -$1.1 only because of the regulating-rod interlock. A latched manual trip must never be clearable while its cause is asserted.

**Fix:** Treat the manual input as a present condition and evaluate reset after it:
```cpp
uint8_t presentNow = present_ | (in.scram ? TRIP_MANUAL : 0);
if (in.scram) latched_ |= TRIP_MANUAL;
if (in.reset && latched_ != 0 && !in.scram)
  tryReset(latched_, fmaxf(rod_, in.rodDemand), presentNow);
```
Also OR `TRIP_MANUAL` into `present` when `in.scram` is true, so that `present_` reflects it.

## Warnings

### PHY-02: A NaN `rodDemand` bypasses the reset interlock and permanently poisons `rod_`

**File:** `lib/reactor/src/reactor.cpp:35`, `lib/reactor/src/plant.cpp:15-22`
**Problem:**
- **Reset interlock bypass.** `rod_ > in.rodDemand ? rod_ : in.rodDemand` evaluates to NaN when the demand is NaN. Then `rodPos >= 0.01f` is false, so **reset is allowed with the rod at any position**.
- **Permanent poisoning.** `moveRod` also lets NaN through: both clamp comparisons are false, so `dx` becomes NaN and `rod_` becomes NaN. From then on, `rodWorth` is NaN, so ρ is NaN, so `stepImplicit` returns false and FAULT latches. Scram cannot recover, because `moveRod(NaN, 0, …)` stays NaN. The fault is unrecoverable without `init()`.

**Evidence:** IEEE comparisons with NaN are false. I traced lines 35, 41-44, 49-50 and 53.
**Fix:** Sanitize at the facade boundary: `float d = isfinite(in.rodDemand) ? in.rodDemand : 0.0f;`. Use `fmaxf` for the reset position. In `moveRod`, write the clamp as `if (!(demand >= 0)) demand = 0;` so that NaN goes to 0.

### PHY-03: A NaN or Inf `Tcool` permanently poisons `dT_`, so the FAULT trip can never be reset

**File:** `lib/reactor/src/reactor.cpp:48,54`, `lib/reactor/src/plant.cpp:25-27`
**Problem:** `pp_.Tcool = in.Tcool` is copied without validation. A single NaN sample makes `dT_` NaN, and `dT_` is integrated state, so it stays NaN forever. Every later step then has feedback = NaN and ρ = NaN, so `ok` is false and `TRIP_FAULT` is present on every step. `tryReset` can never succeed. A transient bad coolant reading therefore bricks the simulator until a reboot.
**Fix:** Reject non-finite inputs and keep the last good value:
```cpp
if (isfinite(in.Tcool)) pp_.Tcool = in.Tcool;
```
Optionally, also guard `dT_`: `if (!isfinite(dT_)) dT_ = 0` after FAULT handling.

### PHY-04: The period meter is only correct when h ≤ 0.1 s and h divides 0.1 s

**File:** `lib/reactor/src/instruments.cpp:22-25,31,34`
**Problem:** `update` takes at most one sample per call and subtracts `SAMPLE_DT` once. With h > 0.1 s (for example a coarse `--dt` in sim_cli):
- `acc` grows without bound: +(h - 0.1) per call.
- Consecutive samples are h apart, but the slope is still divided by `WINDOW = 1.0 s`.

The period is then wrong by a factor of 10·h. For example, at h = 0.25 s it reads 2.5× too short, which can cause spurious SHORT_PERIOD trips. Because `acc` keeps growing over a long run, float resolution there degrades too.
**Fix:** Loop while `acc >= SAMPLE_DT` (sampling the same n), or assert or clamp `h <= SAMPLE_DT` in `Reactor::step` and reject a larger h in the CLI.

## Info

### PHY-05: The dollar unit is inconsistent

`params.h:8` defines `DOLLAR = 0.0065`, but rhoDollars and the prompt-critical trip use Σβ = 0.006502 (`reactor.cpp:61`). As a result, the $2.2 rod and $4 bank are 0.03 % under their nominal worth, and "$1.00" is not exactly prompt critical. **Fix:** define the dollar from `totalBeta(keepinU235())`, or set DOLLAR = 0.006502f.

### PHY-06: `Readings.t` loses 10 ms resolution after about 36 h

`reactor.cpp:58` rebuilds the time as a float. The float ulp reaches 0.0156 s above 131072 s, which is about 36.4 h. Above that point, logged or streamed t stops resolving individual steps, even though `tWhole_` is exact. The rounding bias in `tFrac_ += 0.01f` (0.01f = 0.0099999998) also adds about 0.08 ms/h of drift. **Fix:** expose `tWhole_` and `tFrac_` separately in `Readings`, or use an integer step counter × h.

### PHY-07: SUR is not 0 at steady state

`sur(PERIOD_INF)` returns 2.6e-8 instead of 0 (`instruments.cpp:39`). This is harmless in a display, but it pollutes CSV exports and equality tests. **Fix:** `return period >= PERIOD_INF ? 0.0f : 26.06f / period;`.

### PHY-08: There is no period protection during the 1 s warm-up

The meter warms up for 1 s after `init`, and during that time the period reads INF (`instruments.cpp:28-31`). Only the power and prompt-critical trips protect the first second. This is acceptable for an equilibrium start, but it should be documented. Alternatively, fall back to the slope over `count` samples once `count >= 3`.

### PHY-09: Solver accuracy changes discontinuously at ρ = β

The solver uses 1 substep for ρ < β and 10 substeps for ρ ≥ β (`kinetics.cpp:22`). Just below β, the h·ω for h = 10 ms is not much smaller than just above it, so the backward-Euler phase error jumps by about 10× across the switch. **Fix:** choose the substep count from `h·max(0, (ρ - β + Σλβ)/Λ)` instead of a hard switch, or start substepping at about 0.9β.

### PHY-10: `isValid` checks too few parameters

`plant.cpp:31` checks only alphaT, MC and K. It does not check:
- `rhoCrit == 0.5·rhoW` (needed so that x = 0.5 is critical)
- `rhoS > 0`, `rodSpeed > 0`, `scramSpeed > 0`
- `S >= 0`
- `Tref == Tcool + 1/K` (the equilibrium at n = 1 that `init` assumes)

A custom `PlantParams` can therefore start off-equilibrium or produce scram rods that never move. **Fix:** extend `isValid` with these checks.

---
_Reviewer: gsd-code-reviewer (physics/numerics)_
