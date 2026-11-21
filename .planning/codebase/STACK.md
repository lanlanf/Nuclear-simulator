# Technology Stack

**Analysis Date:** 2026-11-21

## Languages

**Primary:**
- C++ (C++11, set by `CMakeLists.txt`: `CMAKE_CXX_STANDARD 11`, extensions OFF). Used in `lib/reactor/src/` (physics), `src/` (firmware), `desktop/sim_cli.cpp` and `tests/`.
  - Physics code uses `float` only, so it matches the Cortex-M4F single-precision FPU. `-Wdouble-promotion -Wfloat-conversion` enforce this.
  - `README.md` says "Needs a C++17 compiler", but the build only needs C++11. Keep new library code C++11-compatible so it builds on both targets.

**Secondary:**
- Python 3. Used in `tools/reference.py` (SciPy reference solver), `tools/compare.py` (validation plots) and `tools/capture_serial.py` (serial logger).

## Runtime

**Environment:**
- Firmware: Arduino UNO R4 Minima (Renesas RA4M1, Arm Cortex-M4F, 48 MHz, 32 KB RAM, 256 KB flash). Arduino framework, bare-metal `setup()`/`loop()` with no RTOS.
- Desktop: native Windows executables built with MinGW g++. Linked with `-static` when `MINGW` is set, so the `.exe` files run without MinGW on PATH.
- Python: a local venv at `.venv/` (git-ignored).

**Package Manager:**
- PlatformIO resolves firmware libraries through `lib_deps` in `platformio.ini`. No lockfile, and versions are not pinned.
- pip uses `tools/requirements.txt`. Versions are not pinned and there is no lockfile.
- C++ desktop: none. doctest is vendored as `third_party/doctest.h`.

## Frameworks

**Core:**
- Arduino framework on PlatformIO platform `renesas-ra`, board `uno_r4_minima` (`platformio.ini`).
- Shared physics library `lib/reactor/src/` (`kinetics`, `plant`, `protection`, `instruments`, `reactor`, `params.h`) is plain C++ with no Arduino dependencies. PlatformIO finds it automatically through the `lib/` convention. CMake globs it into a static library `reactor`.

**Testing:**
- doctest (single header, vendored at `third_party/doctest.h`). Test entry point is `tests/test_main.cpp`, and the suites are `tests/test_kinetics.cpp`, `tests/test_plant.cpp` and `tests/test_reactor.cpp`. It is registered with CTest as `unit_tests`.
- Python validation: `tools/compare.py` runs `build/sim_cli.exe` and compares the output with the `tools/reference.py` SciPy solution. It writes `figures/*.png`.

**Build/Dev:**
- PlatformIO Core / VS Code PlatformIO IDE: `pio run`, `pio run -t upload`, `pio device monitor`.
- CMake ≥ 3.16 + Ninja + MinGW g++: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release`, then `cmake --build build`.
- VS Code recommended extensions (`.vscode/extensions.json`): `platformio.platformio-ide`, `ms-vscode.cmake-tools`.

## Key Dependencies

**Critical (firmware, `platformio.ini` `lib_deps`, unpinned):**
- `adafruit/Adafruit SSD1306`: OLED driver used by `src/display.h` / `src/display.cpp`.
- `adafruit/Adafruit GFX Library`: graphics/text base for SSD1306.
- `arduino-libraries/Servo`: rod-position servo in `src/panel_io.h`.
- `Wire` (I2C) comes from the Arduino core. It is passed as `&Wire` to `Adafruit_SSD1306`.

**Desktop/test:**
- doctest (vendored, included as a SYSTEM include so its warnings are suppressed).

**Python (`tools/requirements.txt`, unpinned):**
- `numpy`, `scipy` (reference ODE solve in `tools/reference.py`), `matplotlib` (Agg backend in `tools/compare.py`), `pyserial` (`tools/capture_serial.py`).

## Configuration

**Environment:**
- No environment variables or `.env` files are used.
- Runtime behavior is controlled by compile-time constants: physics parameters in `lib/reactor/src/params.h`, timing in `src/main.cpp` (`DT_MS = 10`, `PRINT_MS = 100`, `DRAW_MS = 200`, `ACC_CAP_MS = 250`, `MAX_STEPS = 10`), and pins and pot ranges in `src/panel_io.cpp`.
- Serial baud is 115200 (`monitor_speed` in `platformio.ini`, `Serial.begin(115200)`, and the `--baud` default in `capture_serial.py`).

**Build:**
- `platformio.ini`: single env `[env:uno_r4_minima]`. It sets `build_src_flags = -Wall -Wextra -Wdouble-promotion -Wfloat-conversion -Wshadow`. These flags apply only to project sources, because the Arduino core does not compile warning-free.
- `CMakeLists.txt`:
  - `reactor` and `sim_cli` use the same five warning flags as the firmware.
  - `unit_tests` uses only `-Wall -Wextra -Wshadow`, because the tests compute double-precision references on purpose.
  - Sources are collected with `file(GLOB ... CONFIGURE_DEPENDS)`, so new `lib/reactor/src/*.cpp` and `tests/*.cpp` files are picked up automatically.
- `.gitignore`: `build/`, `cmake-build-*/`, `.pio/`, `.venv/`, `__pycache__/`, `results/*.csv` and `.vscode/*` (except `extensions.json`).

## Platform Requirements

**Development:**
- Windows (PowerShell commands in `README.md`). Needs CMake ≥ 3.16, Ninja, MinGW g++, PlatformIO and Python 3 with a venv.
- On networks that intercept TLS, install `pip-system-certs` into PlatformIO's Python (`README.md`).

**Production:**
- Firmware is flashed to an Arduino UNO R4 Minima over USB (`pio run -t upload`). There is no hosted deployment.

---

*Stack analysis: 2026-11-21*
