# Project log

## 2026-11-20: first full code drop

**Built:** physics library, 23 unit tests, `sim_cli`, SciPy reference and
comparison tools, and the firmware.

- The desktop build has no warnings and 23/23 tests pass.
- The firmware compiles for the UNO R4 Minima with no warnings in project code.
  It uses 16 % of RAM (5.2 kB) and 29 % of flash (76 kB).
- It has not been run on hardware yet.

### Validation (`tools/compare.py`, C++ float at h = 10 ms vs SciPy Radau)

| Case | Max relative error in n |
|---|---|
| step +$0.1 | 7.0e-4 |
| step +$0.5 | 9.9e-3 |
| step −$5 | 8.0e-4 |
| ramp +$0.5 over 10 s | 8.5e-3 |
| rod pull +$0.2 with feedback | 4.1e-4 |

- The error converges at first order: the log-log slope is 1.02.
- Inhour periods: +$0.1 → 98.75 s, +$0.5 → 5.80 s, −$5 → −81.2 s.
- **Scram from full power:** n = 0.135 at 2 s, ρ = −$5.1, period −78 s at 300 s.
- **Startup scenario:** no trips. The shortest period was 11.1 s (trip at 10 s),
  and power settled at n = 0.619, T = 308.6 K as predicted.

### Findings

- **Explicit Euler limit.** The plan said Δt < 3 ms. The correct value is
  2Λ/(β−ρ) ≈ 31 ms at ρ = 0 (an arithmetic slip). The limit is about 5 ms after
  a −$5 SCRAM. Test T6 now runs h = 40 ms (diverges) against 20 ms (stable). The
  plan and the code guide are corrected.
- **Feedback overshoot.** The plan expected 10–20 %. A +$0.2 pull actually peaks
  at n ≈ 3.39 at 60 s, about 81 % above the final 1.87. It is within 2 % of the
  final value by about 530 s, and the SciPy reference agrees. On the panel, this
  pull trips on high power (1.20).
- **Float stagnation in the thermal model.** Integrating T itself at about 320 K
  in `float` rounds away the change in each 10 ms step whenever the power
  imbalance is under about 0.8 %. The model now integrates dT = T − Tref.
- **Period meter noise.** Smoothing ω = 1/τ instead of τ avoids spikes near
  critical. The meter reports 1e9 s when |ω| < 1e-4.

### Design choices

- A reset needs the rod pot fully in as well as the rods, so the reactor can't
  jump back to the pot setting.
- Serial `r <dollars>` test mode bypasses the power, temperature and period
  trips (not prompt critical, fault or manual). It ends on any trip.
- The OLED uses 5 lines because the planned line 3 didn't fit in 21 characters.
  Line 5 shows the state and is inverted when tripped.
- The coolant pot covers 280–300 K, with 290 K at mid-scale.

### Next

- Wire the panel and run `b` (the benchmark) on the board.
- Capture a board run with `tools/capture_serial.py` and compare it with
  `sim_cli`.
- Decide whether to reduce the overshoot (for example a smaller MC) or keep it
  as a teaching point.
