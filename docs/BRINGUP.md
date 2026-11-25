# Hardware bring-up checklist (HW-01..03)

## 1. Before power

- [ ] Parts on the bench match docs/wiring.svg (ELEGOO UNO R4 WiFi, 2 pots, 2 buttons, active buzzer + NPN, 4 LEDs with 1 kΩ, servo + 2 × 470 µF, SSD1306 OLED).
- [ ] Upload: `.\.venv\Scripts\pio.exe run -e uno_r4_wifi -t upload`
- [ ] Monitor: `.\.venv\Scripts\pio.exe device monitor` (115200 baud)
- [ ] Banner `# reactor simulator ready ...` appears (or `# no OLED found, continuing without it` if the OLED is not wired).

## 2. Panel elements

| Element | Pin | Action | Expected | Pass |
|---|---|---|---|---|
| Rod pot | A0 | Turn full range, then back to centre | Plotter `dem:` goes 0 → 100 smoothly (`rod:` only follows at 205 s per stroke) | [ ] |
| Coolant pot | A1 | Turn full range | Plotter `Tc_minus_300:` goes −15 → −5 | [ ] |
| SCRAM button | D2 | Press | Red LED on, rods insert, OLED `TRIP: MANUAL SCRAM` | [ ] |
| Reset button | D3 | Press after rods are in | Trip clears | [ ] |
| Buzzer (active, via NPN) | D4 | Trip; approach a warning | Sounds on trip until reset; short chirp on warning | [ ] |
| Green LED | D5 | Normal running | On when not tripped and no warning | [ ] |
| Yellow LED | D6 | Approach a setpoint | On during warning | [ ] |
| Red LED | D7 | Trip | On while tripped | [ ] |
| Blue LED | D12 | Raise rods to critical | On while critical | [ ] |
| Servo | D9 | Turn rod pot; SCRAM | Follows `rod:` slowly (0–100 % = 0–180°); full insertion on SCRAM | [ ] |
| OLED (I2C) | A4 SDA / A5 SCL, addr 0x3C | Power up; trip | Five lines; line 5 inverted while tripped | [ ] |

## 3. Rod pot steadiness (D-07)

- [ ] Knob at rest, plotter mode, watch `dem:` for 30 s: swing stays under 0.11 % of travel (≈ $0.02).
- [ ] Secondary only: `rho_cents` steady (it also moves with temperature feedback).
- Reference: +$0.2 ≈ 1.1 % of travel; full stroke ≈ 205 s.

## 4. SAFE-01 on board

- [ ] Open the monitor, then pause/stop reading it (port left open).
- [ ] Press SCRAM → servo inserts rods and LEDs change anyway.
- [ ] Resume the monitor.

## 5. SAFE-03 on board

- [ ] Trip, hold SCRAM down, press reset → stays tripped.
- [ ] Release SCRAM, press reset → trip clears.

## 6. HW-03 timing

- [ ] Send `b` before tripping → `# error: b only while tripped`.
- [ ] Send `s` to trip, then `b`.
- [ ] Copy the line `# stepImplicit: X us per step (n = ...), draw: Y us`.
- One OLED frame takes ≥ 23 ms at 400 kHz, so a single draw can never fit in one 10 ms step; the accumulator catches up afterwards. The budget is therefore:
- [ ] Keeps real time: 20·X + Y < 200000 us (20 steps + 1 draw per 200 ms frame). Pass [ ]
- [ ] SCRAM latency: Y < 40000 us (worst case a press waits for one draw + one step, < 50 ms). Pass [ ]
- [ ] Add a dated docs/LOG.md entry "Phase 3 hardware bring-up (HW-01..03)" with X, Y, pass/fail and any mitigation (drawing less often, or a 1 MHz `clkDuring` in src/display.h if the module tolerates it).
