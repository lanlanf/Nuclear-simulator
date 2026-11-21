---
area: python-tools-and-sim-cli
reviewed: 2026-11-20T00:00:00Z
depth: deep
files_reviewed: 5
files_reviewed_list:
  - desktop/sim_cli.cpp
  - tools/reference.py
  - tools/compare.py
  - tools/capture_serial.py
  - tools/requirements.txt
cross_checked:
  - lib/reactor/src/params.h
  - lib/reactor/src/plant.cpp
  - lib/reactor/src/kinetics.cpp
  - lib/reactor/src/instruments.cpp
findings:
  critical: 0
  warning: 9
  info: 10
  total: 19
status: issues_found
---

# Tools / sim_cli: Code Review Report

**Depth:** deep · **Status:** issues_found

## Summary

**The validation is not circular, and the model matches.** I checked `reference.py` against the C++ term by term. These all agree:

- **Kinetics:** the six-group point-kinetics ODE, with the source applied only to rodpull.
- **Rod curve:** the S-curve `RHO_W*(x - sin(2πx)/2π) - RHO_CRIT`.
- **Rod motion:** `min(0.5 + v·t, x_target)`, which is equivalent to `moveRod`.
- **Feedback:** `ALPHA_T*dT`.
- **Thermal model:** `d(dT)/dt = (n - K(dT + Tref - Tcool))/MC`. This is the continuous form of `stepTemperature`'s backward Euler: `(dT + h/MC (P - K(Tref-Tcool)))/(1 + hK/MC)`.
- **Constants:** all match `params.h`. I compared `BETA_I`, `LAMBDA_I`, Λ, DOLLAR, rhoW, rhoCrit, rodSpeed, MC, K, Tcool, Tref, alphaT and S.

These are independent formulations: an adaptive Radau solver in double precision against a fixed-step backward Euler in float.

**Runs:**

- `compare.py` (exit 0) printed these maximum relative errors in n:
  - step +0.1$: 7.01e-4
  - step +0.5$: 9.93e-3
  - step -5$: 8.00e-4
  - ramp +0.5$: 8.50e-3
  - rodpull +0.2$: 4.12e-4
- The convergence slope was **1.02**.
- `git status figures` was clean after the run, and I ran `git checkout -- figures` anyway.
- Stable period from `--inhour 0.1`: 98.751 s. sim_cli's period meter reads 98.33 s at t ≈ 107 s.

**Most of the defects are in input handling, not in the physics.** The main problems are:

- sim_cli accepts bad arguments silently, and one input makes it crash.
- compare.py never fails, so it cannot act as a gate.
- reference.py crashes on edge inputs.
- capture_serial does not check line integrity.

## Warnings

### TL-01 (WARNING): sim_cli ignores a flag that has no value
**File:** desktop/sim_cli.cpp:54
**Problem:** The loop condition is `i + 1 < argc`. A trailing flag with no value (`--scenario step --dt`) is skipped, and the run continues with defaults. Missing values are therefore never reported.
**Evidence:** `sim_cli --scenario step --dt` printed a full 300 s run at dt = 0.01, with exit code 0.
**Fix:** Loop `for (int i = 1; i < argc; i += 2)`. Inside the loop, `if (i + 1 >= argc) { fprintf(stderr, "missing value for %s\n", argv[i]); return 2; }`.

### TL-02 (WARNING): Numeric flags are not validated
**File:** desktop/sim_cli.cpp:56-58, 81
**Problem:** `strtof`/`strtod` run with no end-pointer check, so non-numeric input is accepted.
**Evidence:**
- `--rho-dollars abc` produced a 0 $ step, with exit code 0.
- `--t-end -5` printed only the t = 0 row, with exit code 0.
- `--t-end nan` falls back to the default.
- A very large `--t-end/--dt` value overflows `long steps`.
**Fix:** Add a helper `parseNum(const char*, double*)` that checks `end != s && *end == 0 && isfinite(v)`. Then require `tEnd >= 0` and `tEnd/dt < 1e9`, and return 2 with usage otherwise.

### TL-03 (WARNING): An unknown `--method` value silently runs the implicit method
**File:** desktop/sim_cli.cpp:69
**Problem:** Any value other than `"explicit"` selects implicit. A typo such as `Explicit` or `euler` therefore produces implicit output under the wrong label. That would mislabel stability.png.
**Evidence:** `--method Explicit` ran and exited 0. Its output (n = 1.1131952 at 0.1 s) matches the implicit run.
**Fix:** Accept only `implicit` or `explicit`. For anything else, print usage and return 2.

### TL-04 (WARNING): An unknown scenario prints the CSV header to stdout before the usage error
**File:** desktop/sim_cli.cpp:84 (header) vs scenario check at ~138
**Problem:** The scenario is validated only after the header is printed and the kinetics and rodpull branches are skipped. An unknown scenario therefore writes `t,rho,...` to stdout, then exits 2. A redirect like `> results/x.csv` leaves a valid-looking file containing only a header. The same happens with the startup lead-in trip (`return 1`).
**Evidence:** `--scenario bogus` printed `t,rho,n,T,period,sur,rod,trip` and then the usage text.
**Fix:** Validate the scenario against the six names right after argument parsing, before line 84.

### TL-05 (WARNING): Startup crashes with a null pointer when dt > 300
**File:** desktop/sim_cli.cpp:153-160
**Problem:** `r` is set only inside `for (i < (long)(300/dt))`. When dt > 300 the loop runs zero times, and `r->trips` dereferences nullptr. The 3000 s cool-down also truncates (`(long)(3000.0f/dt)`). For dt values that do not divide it, the lead-in is shorter than documented.
**Evidence:** `--scenario startup --dt 500 --t-end 0` exited with -1073741819 (0xC0000005 access violation) and printed nothing.
**Fix:** Cap dt (for example `dt <= 1.0`, which also keeps the period meter's 0.1 s sampling meaningful). Use `lround`/`ceil` for the lead-in step counts, and always step at least once.

### TL-06 (WARNING): An out-of-range rodpull reactivity diverges silently between sim_cli and reference.py
**File:** desktop/sim_cli.cpp:29-36, 112; tools/reference.py:57
**Problem:** The rod can add only between 0 and +1.0997 $ (`rodWorth(1) = 0.5·rhoW`). If `rho` is outside that range, the two programs disagree:
- `rodFor` bisects silently to the endpoint. `--rho-dollars 5` drives the rod to 1.0, and `-1` keeps it at 0.5. Both exit 0.
- `rod_for` raises `ValueError: f(a) and f(b) must have different signs`.
**Evidence:** I ran both commands. sim_cli `rodpull 5` showed rod = 0.502 and rising, with exit 0. reference.py printed a traceback and exited 1.
**Fix:** In both tools, reject `rho_dollars` outside `[0, rodWorth(1)/beta]` with a clear message and exit code 2. In reference.py, use `ap.error`.

### TL-07 (WARNING): compare.py has no pass/fail criterion and always exits 0
**File:** tools/compare.py:53, 76, 113-119
**Problem:** The error table and slope are only printed. A regression, for example a 10× error increase or a slope of 0.3, still exits 0. That means CI or a pre-submit check cannot use this as validation.
**Evidence:** The run exited 0. There is no threshold anywhere in the file.
**Fix:** Give each case a tolerance (for example `1.5×` the current values: 1e-3, 1.5e-2, 1.2e-3, 1.3e-2, 6e-4). Also require `0.9 < slope < 1.1`. Collect the failures, print them, and `sys.exit(1)` if there are any.

### TL-08 (WARNING): capture_serial writes partial and corrupt lines as data
**File:** tools/capture_serial.py:37-40
**Problem:** On timeout, `readline()` returns a partial line. The only filter is `line[:1].isdigit()`, so these are also written:
- truncated rows;
- the tail of a row that started before `reset_input_buffer`;
- rows with `\ufffd` replacement characters;
- numeric non-CSV messages.

The end time is also checked only between reads, and the header is hard-coded. If the firmware column set changes, for example startup's `inv_m`, the rows no longer match the header and nothing notices.
**Fix:** Write a line only if `raw.endswith(b"\n")` and `len(line.split(",")) == HEADER.count(",") + 1` and every field parses with `float()`. Count the rejected lines and report them. Prefer to adopt the board's own `t,...` header line when one arrives.

### TL-09 (WARNING): requirements.txt is unpinned
**File:** tools/requirements.txt:1-4
**Problem:** No versions are pinned. The validated environment is numpy 2.5.5, scipy 1.18.1, matplotlib 3.11.2 and pyserial 3.5. A future SciPy Radau or brentq change could shift the reference silently, and the figures would not be reproducible.
**Fix:** `numpy==2.5.5`, `scipy==1.18.1`, `matplotlib==3.11.2`, `pyserial==3.5`. At minimum use `>=` lower bounds plus a `python_requires`/README note for 3.12.

## Info

### TL-10 (INFO): reference.py crashes with an IndexError when --t-end <= 0
**File:** tools/reference.py:86-99
**Evidence:** `--t-end 0` and `--t-end -5` both raise `IndexError: list index out of range` (exit 1). Fix: `if args.t_end <= 0: ap.error("--t-end must be > 0")`.

### TL-11 (INFO): inhour_root's bracket fails for tiny reactivity and returns inf for NaN
**File:** tools/reference.py:45
**Evidence:** `--inhour 1e-9` raises `ValueError: f(a) and f(b) must have different signs`. `--inhour nan` prints `inf` with exit 0. Fix: Widen the lower bracket to 1e-12, or for |rho| below ~1e-9 return the asymptotic period `(Λ + Σβi/λi)/rho`. Reject non-finite input.

### TL-12 (INFO): The relative error has no floor for n near 0
**File:** tools/compare.py:41-43
**Problem:** `|sim - ref| / ref` grows without bound as `n_ref → 0`, which would happen with deep negative steps over longer runs or with scram cases. The current cases reach n ≳ 5e-3, so the results are unaffected today.
**Fix:** `/ np.maximum(n_ref, 1e-6)`. Alternatively, report the error in `log(n)` for negative cases.

### TL-13 (INFO): Timestamp alignment depends on dt dividing 0.1 s
**File:** desktop/sim_cli.cpp:82; tools/compare.py:42
**Problem:** `every = lround(0.1/dt)`:
- dt = 0.04 outputs every 0.12 s; I observed rows at 0.12 and 0.24.
- dt = 0.03 outputs every 0.09 s.
- Rounding to `%.4f` makes `t` collide when dt < 5e-5.

`np.interp` then evaluates the reference between its 0.1 s nodes, which adds linear-interpolation error to the measured error. All current compare.py dts divide 0.1, so the effect only reaches the stability plot.
**Fix:** Make sim_cli reject a dt that does not divide 0.1 (`fabs(every*dt - 0.1) > 1e-6`). Alternatively, pass sim times to `reference.simulate` as `t_eval`.

### TL-14 (INFO): The convergence fit is a plain least-squares fit over all points
**File:** tools/compare.py:73-76
**Problem:** The slope (1.02) is fit across 5 points with a ratio of 2 between them. The finest point combines discretization error with float32 round-off from 16 000 steps. If round-off ever dominates, the slope flattens and the fit gives no warning.
**Fix:** Also print the local slopes `diff(log err)/diff(log dt)`, and assert that each one is within [0.8, 1.2].

### TL-15 (INFO): compare.py tracebacks when sim_cli is missing or fails
**File:** tools/compare.py:37
**Problem:** A `FileNotFoundError` or `CalledProcessError` produces a raw traceback, and sim_cli's stderr is lost.
**Fix:** Check `os.path.exists(args.sim)` first, and print `e.stderr` on failure.

### TL-16 (INFO): capture_serial does not handle port errors or Ctrl-C cleanly
**File:** tools/capture_serial.py:28-41
**Problem:**
- `SerialException` (wrong or busy COM port) produces a traceback.
- Ctrl-C closes the file through `with`, but it skips the "wrote N rows" report and leaves a traceback.
- The board is left in CSV mode.
- A negative `--seconds` value is accepted.

**Fix:** Wrap the work in `try/except serial.SerialException` and `KeyboardInterrupt`, print the summary in `finally`, and exit 1 on a port error.

### TL-17 (INFO): The fixed 2 s sleep is a guess for R4 USB enumeration
**File:** tools/capture_serial.py:29
**Problem:** The R4's native USB-CDC does not auto-reset the way a Uno does through DTR. After a firmware upload or reset, though, the COM port can re-enumerate, and the first `write` can be lost. Nothing confirms that `m c` took effect.
**Fix:** After sending `m c`, wait for the firmware's acknowledgement or header line (with a timeout), and retry once.

### TL-18 (INFO): The sim and reference period definitions differ
**File:** tools/reference.py:96; lib/reactor/src/instruments.cpp
**Problem:** The reference uses the instantaneous `dn/n`. The sim uses a 0.1 s-sampled, windowed and EMA-smoothed meter. The `period`/`sur` columns therefore cannot be compared directly, although compare.py only compares `n`. The sim also prints `sur = 2.606e-08` instead of 0 at PERIOD_INF.
**Fix:** Document this in the CSV docstrings. If period is ever compared, compare it only after the meter has settled.

### TL-19 (INFO): DOLLAR (0.0065) and BETA (0.006502) are mixed
**File:** lib/reactor/src/params.h:8; tools/reference.py:21-23
**Problem:** Rod worth is defined in DOLLAR units, but CSV `rho` is divided by the summed β. So `RHO_W` is 2.1993 $, not 2.2 $. Both tools are consistent, so this has no validation impact, but the labels are off by 0.03 %.
**Fix:** Define DOLLAR as `totalBeta`, or note the convention.

---
_Reviewer: gsd-code-reviewer · Depth: deep_
