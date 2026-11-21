---
phase: tests-quality
reviewed: 2026-11-20T20:00:00Z
depth: deep
files_reviewed: 4
files_reviewed_list:
  - tests/test_main.cpp
  - tests/test_kinetics.cpp
  - tests/test_plant.cpp
  - tests/test_reactor.cpp
findings:
  critical: 3
  warning: 13
  info: 7
  total: 23
status: issues_found
---

# Test Suite Review: Quality and Coverage Against the Spec

**Reviewed:** 2026-11-20
**Depth:** deep (source read, spec cross-check, mutation run)
**Baseline:** `cmake --build build; .\build\unit_tests.exe`: 23 test cases, 1892 assertions, all pass. T12 overshoot is 81.28 %.
**Spec:** CODE-GUIDE.html T1–T18, PLAN.md §6 V1–V8.

## Summary

The kinetics tests (T1–T8, T16) are strong. They anchor to independent numbers (55–57 s, 98 s, 5.8 s, −80.6 s, 1/6, F13), and their tolerances have reasonable margins. The facade and protection tests are weak. I ran 32 single-line mutations against `lib/reactor/src` in a temp copy. **26 of 32 survived.** Five were killed, and one (M30) changes no observable behavior and has no test, so its survival is expected. The surviving mutations include:

- flipping the sign of temperature feedback in `Reactor::step`
- ignoring `TRIP_FAULT`
- deleting the high-temperature trip
- raising the power-trip setpoint to 150 %
- removing prompt-critical substepping

The suite proves the physics kernels are right. It does not prove that the shipped `Reactor` facade wires those kernels together correctly, or that it protects the plant.

### Mutation results (temp copy, Release build)

| ID | Mutation | Result |
|---|---|---|
| M01 | `reactor.cpp:51` drop `+ feedback(dT_, pp_)` | **SURVIVED** |
| M31 | `reactor.cpp:51` `+ feedback` → `- feedback` (positive coefficient) | **SURVIVED** |
| M02 | `reactor.cpp:48` drop `pp_.Tcool = in.Tcool` | **SURVIVED** |
| M03 | `protection.cpp:11` delete HIGH_TEMP trip | **SURVIVED** |
| M28 | `params.h:66` powerTrip 1.20 → 1.50 | **SURVIVED** |
| M27 | `protection.cpp:13` `>= 1.0f` → `> 1.05f` | **SURVIVED** |
| M13 | `reactor.cpp:76` delete `if (!ok) present \|= TRIP_FAULT` | **SURVIVED** |
| M12 | add `TRIP_FAULT` to `OVERRIDE_BYPASS` | **SURVIVED** |
| M07 | `tryReset` ignores `present` | **SURVIVED** |
| M09 | safety bank inserts at `rodSpeed` on scram | **SURVIVED** |
| M26 | `stepImplicit` substeps always 1 | **SURVIVED** |
| M25 | `MC` 5 → 1.5 (overshoot and timing change) | **SURVIVED** |
| M24 | `moveRod` at half speed | **SURVIVED** |
| M15/M16 | remove demand clamps in `moveRod` | **SURVIVED** (both) |
| M04/M05/M06 | delete WARN_TEMP, delete WARN_PERIOD, drop the period>0 guard on the period warning | **SURVIVED** (all) |
| M08 | `sur` uses `fabsf(period)` | **SURVIVED** |
| M17/M18 | `isValid` ignores MC/K; `alphaT < 0` strict | **SURVIVED** (both) |
| M19 | `logPower` without the N_MIN floor | **SURVIVED** |
| M20/M21/M22/M23 | OMEGA_MIN 1e-2; no half-h sample rounding; no warm-up; EMA = 1 | **SURVIVED** (all) |
| M30 | `init` failure clobbers `latched_` | **SURVIVED** |
| M10 | safety bank withdraws at scram speed | killed (T13) |
| M11 | bypass drops HIGH_TEMP | killed (Override test) |
| M14 | naive `r_.t += h` | killed (full-power hold, t at 600 s) |
| M29 | reset checks `rod_` only | killed (T13) |
| M32 | implicit ignores source S | killed (T16, T8) |

(The harness output also listed T12 as failing in some rows. That was the `MESSAGE` line echoing the test-case header, not a real failure. See TST-22.)

## Coverage Matrix

| Spec ID | Requirement (abridged) | Test case | Status |
|---|---|---|---|
| T1 | Critical hold 60 000 steps, \|n−1\|<1e-4 | `T1 critical reactor holds power for 60000 steps` | covered |
| T2 | +$0.31 prompt jump ≈ 1.44 at 0.1 s | `T2 prompt jump for a +$0.31 step` | covered |
| T3 | +100 pcm period 55–57 s, within 2 % of inhour | `T3 +100 pcm period matches the inhour root` | covered |
| T4 | +$0.1 / +$0.5 periods within 2 % of inhour | `T4 +$0.1 and +$0.5 periods match the inhour root` | covered |
| T5 | −$5 prompt drop 1/6, period ≈ −80 s | `T5 SCRAM to -$5 ...` | covered |
| T6 | Explicit Euler 40 ms diverges, 20 ms bounded | `T6 explicit Euler diverges ...` | covered |
| T7 | Halve h → error halves | `T7 halving h halves the error` | covered |
| T8 | n → −SΛ/ρ | `T8 subcritical with a source ...` | covered |
| T9 | Rod worth 0, ρw/2, ρw | `T9 rod worth ...` | covered |
| T10 | Rod never moves more than v·h per step | `T10 rod rate limit` | weak (TST-08) |
| T11 | T → Tc + P/K | `T11 thermal steady state ...` | covered (isValid part weak, TST-12) |
| T12 | +$0.2: ΔT≈26 K, P≈1.9, ρ within 1 pcm, peak ~80 % at ~60 s, no ringing | `T12 +$0.2 rod pull ...` | weak (TST-07, and TST-01 for the facade) |
| T13 | Latch holds; reset only with rods in | `T13 trip latches; reset only ...` | weak (TST-04, TST-05) |
| T14 | Prompt critical: trips, no NaN, den > 0 | `T14 prompt critical: substeps ...` (kinetics), `T14 prompt critical trips ...` (reactor) | weak (TST-06, TST-02) |
| T15 | Period meter recovers τ within 1 % | `T15 period meter recovers a known period` | weak (TST-09) |
| T16 | −$2 → −$1 doubles the level | `T16 halving a subcritical rho ...` | covered |
| T17 | S-curve vs DOE within 3 %, midpoint half | `T17 rod S-curve vs DOE Example 3` | covered |
| T18 | Overcooling: T → Tref, P → 1.33 | `T18 overcooling ...` | weak (TST-16, TST-11) |
| V1 | Equilibrium 10 min, \|n−1\|<1e-4 | T1 (kinetics); `Reactor holds full power ...` (1e-3) | covered |
| V2 | +0.1 % δk/k → ~57 s, matches inhour | T3 | covered |
| V3 | +$0.1, +$0.5: prompt jump ≈ β/(β−ρ); period within a few % | T4 (periods only); T2 jump at $0.31 only | weak (TST-13) |
| V4 | SCRAM: prompt drop, then ~−80 s period | T5 | covered |
| V5 | Arduino CSV vs SciPy Radau, error plot | none (manual `tools/compare.py`) | missing (TST-23) |
| V6 | Step-size sweep, first order | T7 | covered |
| V7 | Explicit Euler > 31 ms unstable | T6 | covered |
| V8 | +$0.2 settles at 26 K; peak ~80 % (n≈3.4 at 60 s); within 2 % by ~9 min | T12 | weak (TST-07, TST-01) |

**Totals (26 spec items):** covered 17, weak 8, missing 1.

## Reference Helpers: Verdict

- **`inhourOmega`** (test_kinetics.cpp:17) is correct. The function f(ω) = ωΛ + Σβᵢω/(ω+λᵢ) − ρ increases monotonically on (−λ₁, ∞), because each term has derivative βᵢλᵢ/(ω+λᵢ)² > 0. The brackets are valid:
  - For ρ > 0, the root lies in [0, 1000]. The upper bound holds while ρ < 1000·Λ, which is about 15 $.
  - For ρ < 0, f → −∞ as ω → −λ₁⁺, so the root lies in (−λ₁, 0).

  200 halvings is far more than needed. It reads the same float parameters `P` as the code, so it is not independent of a typo in `keepinU235()`. However, T3 and T4 anchor it to literal values (55–57 s, 98 s, 5.8 s, −80.6 s), and that closes the gap.
- **`measuredPeriod`** (:30) is correct. Its windows sit well past the transient.
- **`runPlant`** (test_plant.cpp:17) is correct, and it mirrors `Reactor::step`: rod move, then ρ from rod plus lagged feedback, then kinetics, then temperature. The safety bank is omitted, which is fine because `safetyWorth(1) = 0`. However, it is a **re-implementation** of `Reactor::step`, so T12 and T18 do not exercise the shipped facade. See TST-01.
- **`rodFor`** (:43) is a 40-step bisection on a monotone S-curve over [0.5, 1]. It is correct.
- T7 reference bias: using h = 0.5 ms as the "exact" answer biases the expected ratio to about 1.95/0.95 ≈ 2.05, which is well inside ±20 %. A second-order method would give about 4 and fail, as it should.

## Critical Issues

### CR-01 (TST-01): Temperature feedback in the shipped `Reactor` facade is untested; a sign flip passes

**File:** `tests/test_plant.cpp:17-40`, `tests/test_reactor.cpp:32-123` (code: `lib/reactor/src/reactor.cpp:51`)
**Problem:** T12 and T18 (V8) run through the test-local `runPlant`, not through `Reactor::step`. No facade test leaves the dT = 0 equilibrium while feedback is active:
- the full-power hold sits at dT = 0;
- T13 and the T14 shutdown are deeply subcritical;
- the override tests bypass feedback;
- the fast-withdrawal test trips whether or not feedback is present.

**Evidence:** Mutation M01 (feedback removed) and M31 (`+ feedback` → `- feedback`, which makes the temperature coefficient positive) both **survive**. The firmware could ship with a positive temperature coefficient and every test would pass.
**Test to add:**
```cpp
TEST_CASE("V8 facade: +$0.2 rod pull settles with feedback") {
  Reactor r; REQUIRE(r.init());
  Inputs in; in.rodDemand = 0.5513f;  // ~+$0.2, or reuse rodFor() from a shared header
  // stay below the 1.20 power trip: use a +$0.1 pull, or init with relaxed Setpoints
  Setpoints sp = defaultSetpoints(); sp.powerTrip = 10.0f; sp.periodTrip = 0.0f; sp.tempTrip = 1e9f;
  REQUIRE(r.init(defaultPlant(), sp));
  const Readings& out = runFor(r, in, 1800.0);
  CHECK(out.trips == 0);
  CHECK(out.T - 320.0f == doctest::Approx(26.0).epsilon(0.03));
  CHECK(out.n == doctest::Approx(1.867).epsilon(0.02));
  CHECK(fabs(out.rho) < 1e-5);
}
```

### CR-02 (TST-02): The `TRIP_FAULT` path is never exercised (`stepImplicit` returning false)

**File:** `tests/test_kinetics.cpp:131-140`, `tests/test_reactor.cpp:82-96`, `:114-123` (code: `kinetics.cpp:34`, `reactor.cpp:76-77`)
**Problem:** Every test asserts that the fault bit is **absent**. Nothing drives the denominator ≤ 0 or produces NaN to prove that the fault latches and survives override mode.
**Evidence:**
- M13 (delete `if (!ok) present |= TRIP_FAULT`) **survives**.
- M12 (add `TRIP_FAULT` to `OVERRIDE_BYPASS`, which silently hides a solver failure in test mode) **survives**.

With h = 10 ms and 10 substeps (hs = 1 ms), the denominator 1 − hs(ρ−β)/Λ − … ≤ 0 once ρ − β > 0.1, which is about $16.4.
**Tests to add:**
```cpp
TEST_CASE("stepImplicit rejects a non-positive denominator") {
  Kinetics k; initEquilibrium(k, P, 1.0f);
  CHECK_FALSE(stepImplicit(k, P, 20.0f * 0.0065f, 0.0f, 0.01f));
}
TEST_CASE("Solver failure latches TRIP_FAULT even in override mode") {
  Reactor r; REQUIRE(r.init());
  Inputs in; in.overrideOn = true; in.rhoOverride = 20.0f * 0.0065f;
  const Readings& out = r.step(in, 0.01f);
  CHECK((out.trips & TRIP_FAULT) != 0);
  CHECK(isfinite(out.n));
}
```
Also add a direct `evaluate(NAN, 300, PERIOD_INF, 0, sp) & TRIP_FAULT` check.

### CR-03 (TST-03): Trip setpoints are not pinned; the high-temperature trip is never tested

**File:** `tests/test_reactor.cpp:82-123` (code: `protection.cpp:10-13`, `params.h:66`)
**Problem:** These are the safety functions, and no test checks any threshold:
- The fast-withdrawal test accepts "HIGH_POWER or SHORT_PERIOD" with no time bound.
- The T14 reactor test uses $1.2, which is far from the $1 threshold.
- HIGH_TEMP is never produced or asserted.

**Evidence:** M03 (HIGH_TEMP trip deleted), M28 (power trip 1.20 → 1.50) and M27 (prompt-critical threshold `>= 1.0` → `> 1.05`) all **survive**.
**Tests to add:** direct table tests on `evaluate()` at each edge.
```cpp
TEST_CASE("evaluate: each trip at its setpoint") {
  const Setpoints s = defaultSetpoints();
  CHECK(evaluate(1.20f, 300, PERIOD_INF, 0, s) == 0);
  CHECK(evaluate(1.2001f, 300, PERIOD_INF, 0, s) == TRIP_HIGH_POWER);
  CHECK(evaluate(1.0f, 330.01f, PERIOD_INF, 0, s) == TRIP_HIGH_TEMP);
  CHECK(evaluate(1.0f, 300, 9.99f, 0, s) == TRIP_SHORT_PERIOD);
  CHECK(evaluate(1.0f, 300, -5.0f, 0, s) == 0);           // negative period never trips
  CHECK(evaluate(1.0f, 300, PERIOD_INF, 1.0f, s) == TRIP_PROMPT_CRIT);
  CHECK(evaluate(1.0f, 300, PERIOD_INF, 0.999f, s) == 0);
}
```
Also add a facade test that drives T above 330 K with feedback active and no override (for example, Tcool raised to 325 K at full power), then asserts `TRIP_HIGH_TEMP`.

## Warnings

### WR-01 (TST-04): `tryReset` refusal while a trip condition is present is untested

**File:** `tests/test_reactor.cpp:51-80` (code: `protection.cpp:25-29`)
**Problem:** T13 covers "rods not in" and "rod pot not in". It never resets while the cause of the trip persists. M07 (drop `present != 0`) **survives**. Also, `present_` lags one step (`reactor.cpp:35` uses the previous step's value), and nothing tests that.
**Test:** call `tryReset(latched, 0.0f, TRIP_HIGH_TEMP)` directly and assert `false` with the latch unchanged. In the facade, latch a HIGH_TEMP trip with Tcool high, insert the rods, request reset, and assert `trips != 0` until T drops below 330 K.

### WR-02 (TST-05): Safety-bank insertion speed on scram is untested

**File:** `tests/test_reactor.cpp:51-80` (code: `reactor.cpp:41`)
**Problem:** `Readings` does not expose the safety-bank position, and T13 checks only `rod`. M09 (safety bank inserts at 0.02/s instead of 1/s, which turns a 1 s insertion into 50 s) **survives**.
**Test:** scram from full power, step 1.0 s, then `CHECK(out.rhoDollars < -5.0f + 0.05f)`. The −$5.1 total comes from T9. Also assert that at 0.5 s, `rhoDollars < -2.5`.

### WR-03 (TST-06): The kinetics T14 test does not exercise the substeps it names

**File:** `tests/test_kinetics.cpp:131-140` (code: `kinetics.cpp:22`)
**Problem:** At $1.5 with h = 10 ms, a single backward-Euler step still has den = 1 − 0.325 − … > 0, so substepping is irrelevant to this test. M26 (substeps fixed at 1) **survives**. With one step, den ≤ 0 once ρ − β > Λ/h = 0.01, which is about $2.54.
**Test:** use `3.0f * 0.0065f` (one step would fail, ten substeps succeed) and assert `ok`. Add the companion `CHECK_FALSE` at $20 from CR-02.

### WR-04 (TST-07): The T12 overshoot assertion is vacuous; V8's numeric criteria are not checked

**File:** `tests/test_plant.cpp:89-102`
**Problem:**
- `overshoot > 0.0` passes for any response that ever exceeds its final value, including float noise on a critically damped response.
- The spec (V8, T12) says the peak is ~80 % (n ≈ 3.4) at about 60 s and the response is within 2 % by about 9 min. Neither is asserted.
- `lateSpread` covers only the last third (1200–1800 s).

M25 (MC 5 → 1.5, which changes both overshoot and timing materially) **survives**.
**Change:** return `tPeak` and n at 540 s from `runPlant`, then:
```cpp
CHECK(overshoot == doctest::Approx(0.81).epsilon(0.10));
CHECK(r.tPeak == doctest::Approx(60.0).epsilon(0.25));
CHECK(fabs(r.nAt540 / r.n - 1.0) < 0.02);
```
The 81 % value is cross-checked by `tools/reference.py`, so pinning it to ±10 % is justified and not flaky, because the solver is deterministic.

### WR-05 (TST-08): T10 never checks progress, reaching the demand, or the clamps

**File:** `tests/test_plant.cpp:62-76`
**Problem:** Starting at 0.5 with 100 steps × 2e-4, x stays in [0.48, 0.54], so the `[0,1]` checks can never fail. Nothing asserts that the rod actually moves at v or reaches the demand exactly.
**Evidence:** M15 and M16 (demand clamps removed) and M24 (half speed) all **survive**.
**Test:**
```cpp
CHECK(moveRod(0.5f, 1.0f, 0.02f, 0.01f) == doctest::Approx(0.5002f));
CHECK(moveRod(0.5199f, 0.52f, 0.02f, 0.01f) == 0.52f);       // no overshoot
CHECK(moveRod(0.999f, 2.0f, 1.0f, 0.01f) == 1.0f);           // clamp high
CHECK(moveRod(0.001f, -1.0f, 1.0f, 0.01f) == 0.0f);          // clamp low
```

### WR-06 (TST-09): Period-meter edge cases are untested

**File:** `tests/test_reactor.cpp:16-30` (code: `instruments.cpp:22-38`)
**Problem:** Only the steady-state recovery of τ = 20 s and −50 s at h = 10 ms over 30 s is checked. Untested:
- the 1 s warm-up (`PERIOD_INF` before 11 samples);
- the OMEGA_MIN infinity band (|τ| > 10⁴ s);
- the EMA response time;
- cadence when h does not divide 0.1 s;
- n ≤ 0.

**Evidence:** M20, M21, M22 and M23 all **survive**.
**Tests:**
- After 10 updates of 0.1 s with a rising n, `period == PERIOD_INF`.
- τ = 2×10⁴ s reads `PERIOD_INF`, and τ = 5000 s reads finite.
- After a step change from τ = 20 s to τ = −50 s, the reading after 0.5 s is still between the two values. This catches EMA = 1.
- Repeat the τ = 20 s recovery with h = 0.03 s and h = 0.025 s.
- `update(m, 0.0f, 0.1f)` stays finite.

### WR-07 (TST-10): `warnings()` is tested for WARN_POWER only

**File:** `tests/test_reactor.cpp:41`, `:108` (code: `protection.cpp:17-23`)
**Problem:** WARN_TEMP and WARN_PERIOD are never asserted, and nothing checks that a negative period does not warn. M04, M05 and M06 all **survive**.
**Test:** a table test like the one in CR-03, covering `warnings()` at 1.05, 325 K, 30 s and −30 s.

### WR-08 (TST-11): The facade's `Inputs.Tcool` wiring is untested

**File:** `tests/test_reactor.cpp` (all cases use Tcool = 290; code `reactor.cpp:48`)
**Problem:** M02 (Tcool ignored) **survives**. T18 tests overcooling only through `runPlant`.
**Test:** at the facade level, use `in.Tcool = 280`, run 1800 s with a relaxed power-trip setpoint, then assert `n ≈ 1.333` and `T ≈ 320 K`.

### WR-09 (TST-12): `isValid` and the failed-`init` contract are only partially tested

**File:** `tests/test_plant.cpp:84-87`, `tests/test_reactor.cpp:44-49`
**Problem:** Only `alphaT = +1e-5` is tested. MC ≤ 0, K ≤ 0 and the boundary `alphaT == 0` (which `isValid` should accept) are not. The header promises that `init` "keeps the old state" on failure, and nothing checks that. M17, M18 and M30 **survive**.
**Test:**
- `bad.MC = 0` → false; `bad.K = -1` → false; `alphaT = 0` → true.
- Trip a running `Reactor`, call `init(bad)` and assert false, then step and assert the trip is still latched.

### WR-10 (TST-13): V3's prompt jump for +$0.1 and +$0.5 is not asserted

**File:** `tests/test_kinetics.cpp:61-68`
**Problem:** V3 requires a prompt jump of about β/(β−ρ) for the $0.1 and $0.5 steps (1.111 and 2.0). T4 checks only the periods, and T2 checks only $0.31.
**Test:** after 0.1 s at h = 1 ms, assert `n ≈ 1.11` (±2 %) for $0.1 and `n ≈ 2.0` (±3 %, with delayed growth over 0.1 s at τ = 5.8 s) for $0.5.

### WR-11 (TST-14): The sign of SUR is untested

**File:** `tests/test_reactor.cpp:28` (code: `instruments.cpp:40`)
**Problem:** Only `sur(26.06) == 1` is checked. The spec requires about −1/3 DPM after a scram (PLAN §2.5), and `sur(PERIOD_INF)` should read about 0. M08 (`fabsf`) **survives**.
**Test:** `CHECK(sur(-80.0f) == doctest::Approx(-0.326).epsilon(0.01)); CHECK(fabs(sur(PERIOD_INF)) < 1e-6);` Also assert that `Readings.sur < 0` about 60 s after a facade scram.

### WR-12 (TST-15): Override bypass is not checked per trip type

**File:** `tests/test_reactor.cpp:98-112`
**Problem:**
- The test asserts `trips == 0` overall. It does not show which trips were present and suppressed.
- That HIGH_TEMP is bypassed is covered only by accident: T happens to exceed 330 K within 20 s (M11 was killed).
- That PROMPT_CRIT and FAULT are *not* bypassed is covered by T14 (PROMPT_CRIT only). FAULT is not covered (M12 survived; see CR-02).

**Test:** one assertion per trip bit: drive each condition with override on and assert that the bit is clear. Then turn override off and assert that the bit latches on the next step, as the test already does for HIGH_POWER.

### WR-13 (TST-16): The T18 temperature tolerance is ±0.64 K, too loose for "T → Tref"

**File:** `tests/test_plant.cpp:115`
**Problem:** `Approx(PP.Tref).epsilon(2e-3)` is relative to 320 K, so it allows ±0.64 K. That equals ±3.2 pcm of unbalanced feedback, three times the 1 pcm criterion that T12 uses. The analytic residual is 0 K.
**Change:** `CHECK(fabs(r.T - PP.Tref) < 0.05); CHECK(fabs(r.rho) < 1e-5);`

## Info

### IN-01 (TST-17): Time precision is tested only to 600 s

**File:** `tests/test_reactor.cpp:42`
**Problem:** M14 (naive float accumulation) is killed at 600 s, which is good. However, the `tWhole_`/`tFrac_` design is justified by "after an hour", and no test runs that long.
**Suggestion:** run 3600 s or more (360 k steps, which is cheap) and assert `t == Approx(3600).epsilon(1e-6)`.

### IN-02 (TST-18): `runFor` returns a reference that aliases `Reactor::r_`

**File:** `tests/test_reactor.cpp:10-14`
**Problem:**
- Every `const Readings& x = runFor(...)` refers to the same object, so a later `step` silently changes `x`. The current tests read each value before the next step, which keeps them correct but fragile; `hit` and `later` in T14 are the same object.
- `seconds < 0.005` dereferences `nullptr`.

**Suggestion:** return `Readings` by value.

### IN-03 (TST-19): The fast-withdrawal test accepts either of two trips with no time bound

**File:** `tests/test_reactor.cpp:114-123`
**Suggestion:** record the first step at which `trips != 0`, then assert which bit it was and that the time falls in the expected window.

### IN-04 (TST-20): The $1 boundary differs between `DOLLAR` and `totalBeta`

**File:** tests use `0.0065` and `BETA = 0.0065` (`test_kinetics.cpp:9`). The code's prompt-critical trip divides by `totalBeta(kp_)` = 0.006502.
**Problem:** an override of exactly `DOLLAR` reads as $0.9997 and **does not trip**, and no test documents this. The difference is small but sits right on the boundary.
**Suggestion:** use `totalBeta(P)` in the tests, and add a boundary test at `1.0f * totalBeta(P)`.

### IN-05 (TST-21): The `logPower` floor is untested

**File:** `tests/test_reactor.cpp:29`
**Problem:** M19 survives.
**Suggestion:** `CHECK(logPower(0.0f) == doctest::Approx(-30.0)); CHECK(isfinite(logPower(-1.0f)));`

### IN-06 (TST-22): doctest usage

**File:** `tests/test_plant.cpp:66-74`, `tests/test_plant.cpp:99`
- T10 emits about 1800 `CHECK`s from loops, which accounts for most of the 1892 assertions. One failure would print hundreds of lines. Accumulate a `maxStep` value and check it once, or use `REQUIRE` inside the loop.
- `MESSAGE` in T12 prints a `TEST CASE:` header on every run. Tools that scrape for failures (as my mutation harness did) mistake it for a failure. Prefer `INFO` (printed only on failure) or a `CAPTURE`.
- Two test cases are prefixed "T14", one in each of two files. This is fine, but use `-tc="T14*"` filtering knowingly.
- `doctest::Approx(0.0)` uses a scale-1 absolute tolerance of about 1.2e-5 (T9 at :53 and :57, T11 at :82). That is acceptable here (0.18 cents), but an explicit `.epsilon` or a `fabs(...) <` check states the intent.

### IN-07 (TST-23): V5 (comparison against SciPy Radau) is not automated

**File:** none (`tools/compare.py` runs manually)
**Problem:** No CI or unit check guards the reference overlay, so a regression would show up only if someone regenerates the figures.
**Suggestion:** add a CTest that runs `sim_cli` for one scenario, then `tools/compare.py --max-rel-err 0.02` (skip it if Python is missing). Alternatively, commit a small golden CSV from Radau and compare it in doctest.

## Tolerance Audit (non-findings)

These tolerances are justified and showed no flakiness. The solver is deterministic, so the suite has no source of nondeterminism.

| Test | Tolerance check |
|---|---|
| T1 | 1e-4 |
| T2 | 1.45 ± 2 % |
| T3 / T4 | ± 2 % against inhour, plus literal anchors |
| T5 | 1/6 ± 5 % at 50 ms, −80.6 ± 5 % |
| T6 | 40 ms vs 34 ms limit, 20 ms → 1.2 ± 10 % (expected ≈ 1.11·e^{10/98} ≈ 1.23) |
| T7 | ratio 2 ± 20 %, expected ≈ 2.05 |
| T8 / T16 | 1 % and 0.1 % after ≥ 20 time constants |
| T11 | 1e-4 relative, ≈ 0.03 K, after 13 τ |
| T17 | 3 % of 260 pcm |

---

_Reviewed: 2026-11-20_
_Reviewer: the agent (gsd-code-reviewer)_
_Depth: deep_
