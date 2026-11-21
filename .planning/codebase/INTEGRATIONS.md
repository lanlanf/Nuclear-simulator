# External Integrations

**Analysis Date:** 2026-11-21

## APIs & External Services

**Web APIs / cloud services:**
- None. The project has no network, HTTP, cloud or third-party service integration. All of its integrations are hardware peripherals and a USB serial link.

**Hardware peripherals (control panel):** pins are defined in `src/panel_io.cpp` and documented in `README.md`.

| Pin | Part | Code behavior |
|---|---|---|
| A0 | Rod pot | `analogRead`/1023, EMA filter `POT_EMA = 0.1` → `rodDemand` 0..1 |
| A1 | Coolant pot | EMA → `Tcool = 280 + 20*pot` K (290 K at mid-scale) |
| D2 | SCRAM button | `INPUT_PULLUP`, `attachInterrupt(..., FALLING)` sets a `volatile` flag. It is read and cleared under `noInterrupts()` |
| D3 | Reset button | `INPUT_PULLUP`, software debounce `DEBOUNCE_MS = 30`, press edge only |
| D4 | Buzzer | Stays on while tripped until acknowledged (reset). On a warning it chirps for 100 ms every 1 s |
| D5 | Green LED | Not tripped and no warning |
| D6 | Yellow LED | Warning (`r.warn`) |
| D7 | Red LED | Any trip |
| D12 | Blue LED | Critical: \|rho\| < 5e-5 and \|period\| > 1000 s |
| D9 | Servo | `Servo` library. Angle = `180 * rod`, written only when the angle changes |
| SDA/SCL | SSD1306 OLED | I2C |

- Hardware notes (`README.md`): use a series resistor on each LED. Power the servo from a separate 5 V supply that shares ground with the Arduino.

**I2C OLED (`src/display.h`, `src/display.cpp`):**
- SSD1306 128x64 at address `0x3C`, `&Wire`, no reset pin (`-1`), `SSD1306_SWITCHCAPVCC`.
- If `begin()` fails, the firmware prints `# no OLED found, continuing without it`. `draw()` then does nothing.
- The display redraws at 5 Hz (`DRAW_MS = 200`) in five text-size-1 lines: P% + log P, rho $ + rod %, T K + period s (`inf` if ≥ 1e4), SUR DPM, and a state line.
- The state line is shown inverted while tripped. The trip state priority is FAULT > PROMPT CRIT > SHORT PERIOD > HIGH POWER > HIGH TEMP > MANUAL. If no trip is active, the line shows `TEST: RHO OVERRIDE`, `WARNING`, `SUBCRITICAL`, `SUPERCRITICAL` or `CRITICAL`.

**USB serial (`src/main.cpp`):**
- Baud 115200, 8N1 (Arduino default). `setup()` waits up to 2 s for `Serial`.
- Banner: `# reactor simulator ready. commands: m p|m c|m t, r <dollars>, x, s, z, b`. Informational lines begin with `#`.
- Commands end at `\n` or `\r`. The line buffer holds 32 bytes (31 characters), and longer input is silently truncated. Unknown non-empty commands print the help line.

| Command | Effect |
|---|---|
| `m p` | Arduino Serial Plotter mode (the default) |
| `m c` | CSV mode, prints header `t,rho,n,T,period,sur,rod,trip` |
| `m t` | Teleplot mode |
| `r <dollars>` | Override rod reactivity with `strtof(...) * totalBeta(keepinU235())`. Any trip clears the override |
| `x` | Clear the override |
| `s` | Soft SCRAM (one-shot `in.scram`) |
| `z` | Soft reset (one-shot `in.reset`, also acknowledges the buzzer) |
| `b` | Benchmark: 1000 × `stepImplicit`, prints `# stepImplicit: <us> us per step (n = ...)` |

**Output formats:** printed at 10 Hz (`PRINT_MS = 100`), using the most recent `Readings`.
- Serial Plotter: `P:<100n %.2>,rod:<100rod %.1>,rho_cents:<100rho$ %.2>,T_minus_300:<T-300 %.2>`
- Teleplot: one `>name:value` line per channel: `>P:` (%), `>logP:`, `>rho:` ($), `>T:` (K), `>rod:` (%), `>SUR:` (DPM).
- CSV: `t(2dp),rho$(5),n(8),T(3),period(2),sur(4),rod(4),trips(int)`. The trip bit mask is 1 manual, 2 high power, 4 high temp, 8 short period, 16 prompt critical, 32 fault (`lib/reactor/src/protection.h`).
- `desktop/sim_cli.cpp` writes the same CSV columns to stdout (`startup` adds `inv_m`), so board captures and desktop runs can be compared directly.

**Python serial capture (`tools/capture_serial.py`):**
- `pyserial` `serial.Serial(port, baud=115200, timeout=1.0)`. It waits 2 s after opening because opening the port may reset the board, then clears the input buffer.
- It sends `m c`, plus any `--command` values (repeatable), each terminated with `\n`.
- It writes the header and then only lines that start with a digit, skipping `#` messages and the board's own header. Output goes to `results/board_%Y%m%d_%H%M%S.csv`.
- CLI: `--port` (required, e.g. `COM5`), `--baud`, `--seconds` (default 60).

**Desktop tool chaining:**
- `tools/compare.py` runs `build/sim_cli.exe` through `subprocess` (path can be overridden with `--sim`). It imports `tools/reference.py` and writes `figures/overlay_*.png`, `figures/convergence.png` and `figures/stability.png`.

## Data Storage

**Databases:**
- None.

**File Storage:**
- Local filesystem only. CSV goes to `results/` (git-ignored `*.csv`) and plots go to `figures/`. The firmware keeps no persistent state, so nothing is written to EEPROM or flash.

**Caching:**
- None.

## Authentication & Identity

**Auth Provider:**
- None. The serial port is unauthenticated, so anyone with USB access can override reactivity, SCRAM or reset the simulator.

## Monitoring & Observability

**Error Tracking:**
- None. The OLED and LEDs show trips (including `TRIP_FAULT` for numerical faults).

**Logs:**
- `#`-prefixed serial lines. Telemetry can be viewed in the PlatformIO monitor, the Arduino Serial Plotter, the Teleplot VS Code extension, or a CSV capture.

## CI/CD & Deployment

**Hosting:**
- Not applicable. Firmware is flashed over USB with `pio run -t upload`.

**CI Pipeline:**
- None detected. Tests are run locally with `.\build\unit_tests.exe` or `ctest --test-dir build`.

## Environment Configuration

**Required env vars:**
- None.

**Secrets location:**
- Not applicable. There are no secrets.

## Webhooks & Callbacks

**Incoming:**
- None. Inputs come only from the hardware: the SCRAM pin interrupt on D2 is the only ISR.

**Outgoing:**
- None.

---

*Integration audit: 2026-11-21*
