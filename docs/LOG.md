# Project log

## 2026-11-25: validation fixes (before bring-up)

A deep review before wiring found ten issues. All are fixed; 49/49 test cases
pass and both firmware envs build with no warnings.

**Firmware**

- **No serial output on the R4 WiFi.** There `Serial` is a UART into the USB
  bridge, and `availableForWrite()` always returns 0, so every line was dropped.
  The check is now skipped under `NO_USB`.
- **OLED presence.** Adafruit `begin()` only fails on malloc. The firmware now
  probes 0x3C for an I²C ACK first, so a missing OLED is reported.
- **Period label** shows `inf` near critical instead of a huge number.
- **Plotter** adds `dem` (rod pot, %) and `Tc_minus_300` (coolant pot) so the
  bring-up pot checks don't need a meter.
- **Coolant pot** is now 285–295 K (was 280–300 K). The −10 K end tripped at power.
- **Servo** pulse range is 600–2400 µs. The 544 µs default can stall an SG90 on
  its end stop.
- **Serial:** `r` while tripped prints `# error: r ignored while tripped, reset
  first`. Lines over 31 characters print `# error: command too long` and are
  not executed (before, the truncated text ran).

**Rod stop (new protection feature)**

While any warning is up (power > 1.05, T > 325 K, period < 30 s), `Reactor::step`
blocks rod withdrawal. Insertion always works. This is the "rod withdrawal
block" from real plants.

- A full rod pull from power used to trip on high power. It now holds below
  the trips (rod < 0.55, max n < 1.2).
- A power-on pot offset of ±50 % no longer trips.
- **Startup is corrected.** The 2026-11-20 claim "startup: no trips, n = 0.619"
  predates the retune. With the retuned plant, the old startup schedule tripped
  SHORT_PERIOD at 606.7 s. `sim_cli --scenario startup` now turns the rod pot
  straight to 0.5 and lets the rod stop pace it: no trips, minimum period
  18.9 s, n ≥ 0.95 at 381 s, final n 1.000, T 320.0 K. A regression test covers it.
- Mutants M33 and M34 (rod stop removed or made to block insertion) are killed.

**Docs and plan**

- HW-03 pass rule: the board keeps real time if 20·X + Y < 200 000 µs per
  2 s window, and SCRAM latency is Y < 40 000 µs (< 50 ms). One OLED frame takes
  at least 23 ms at 400 kHz, so the old rule could not pass.
- **Known limit:** the board boots at full power with no trip latched. If it
  resets mid-demo (reboot or brownout), press SCRAM. BRINGUP.md §5 says so.
- **Correction to the retune notes:** $0.2 is about 1.1 % of pot travel, which
  is about 180 counts at 14 bits (not 11 counts at 10 bits).
- README, PLAN.md/.html and CODE-GUIDE.html now use the as-built numbers: $9
  rod, −75 pcm/K, τ 7.5 s, shutdown margin −$8.5 hot, and a +$0.2 pull that
  peaks about 11 % high at 2.3 s and settles at n 1.058, ΔT 1.73 K.

## 2026-11-21: parts list finalized

**Board:** ELEGOO UNO R4 WiFi (starter kit). It has the same RA4M1 chip as the
Minima, so the firmware is unchanged; added the PlatformIO env `uno_r4_wifi`.
It compiles at 19 % RAM (6.3 kB) and 31 % flash (81 kB).

**Amazon order ($71.65):** the ELEGOO kit, Flutesan 10 kΩ trim pots (24-pc),
E-Projects 470 µF 50 V caps (5-pc) and an ELEGOO SSD1306 OLED (3-pc). Links
are in [PLAN.md §4.2](PLAN.md#42-bill-of-materials). The kit has no 470 Ω
resistor, so the LEDs use 1 kΩ; the wiring diagram now shows 2 × 470 µF across
the servo supply.

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

## 2026-11-20: demo retune (Phase 2, TUNE-01..03)

### Parameter changes (`params.h` and `tools/reference.py`)

| Parameter | Old | New | Reason |
|---|---|---|---|
| MC | 5 | 0.25 | Thermal lag τ = MC/K was 150 s, so feedback arrived too late and a +$0.2 rod step overshot to n 1.68–2.16. τ is now 7.5 s. |
| αT | −5e-5 | −7.5e-4 dk/k/K | Holds the +$0.2 rod step and the −5 K coolant step below the 1.20 trip. |
| ρ_w (regulating rod) | $2.2 | $9 | Shutdown margin. A cold core (290 K, ΔT −30 K) now adds +$3.46 of feedback. With ρ_w $2.2 the rod alone gave −$1.1, and the core went critical again after a reset. Now the rod in, cold, with the bank out gives −$1.05. |
| Rod speed | 0.02 /s | 0.0049 /s | = 0.02 × 2.2/9, which keeps the $/s at the critical position the same. At 0.01 /s the rod step trips on period (5.5 s). |

Setpoints, ρ_S, scram speed, K, Tref and Tcool are unchanged.

### Sweep (reference.py, 300 s from full power)

| Case | Peak n | Min period |
|---|---|---|
| αT only (MC 5), rod +$0.2 | 1.68–2.16 | — |
| αT only (MC 5), coolant −5 K | ≈1.213 (all αT) | — |
| MC 0.25, αT −7.5e-4, rod +$0.2 | 1.176 | 11.4 s |
| MC 0.25, αT −7.5e-4, coolant −5 K | 1.167 | 13.2 s |
| ρ_w $9, speed 0.0049: same two cases | 1.176 / 1.167 | 11.4 / 13.2 s |
| ρ_w $9, speed 0.01, rod +$0.2 | — | 5.5 s (trips) |

Grid MC 0.2–0.5 × αT −6e-4..−1.5e-3: the rod-step minimum period is about 11–12 s for every combination. Rod speed limits it, not feedback.

C++ facade, default setpoints: TUNE-01 maxN 1.177, maxT 321.7 K, min period 12.05 s. TUNE-02 maxN 1.167, min period 15.0 s. Neither trips. Cold (rod in, bank out) holds ρ at −$1.05 and power falls for 300 s.

### Notes

- **Period margin is thin:** the 10 s period trip is only about 1.4 s (reference) to 2 s (C++) below the rod-step minimum. Faster rods or a larger step will trip.
- **Hardware (Phase 3):** $0.2 is now about 1.1 % of pot travel, roughly 11 ADC counts at 10 bits. A full stroke takes about 205 s.
