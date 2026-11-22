# Arduino Nuclear Reactor Control & Simulation Panel — Project Plan

ENGR 217 personal project · Landon Fancher · plan drafted 2026-11-18

## 1. Concept

This is a miniature reactor control panel. The reactor physics runs as a real-time math model on the Arduino itself, not on a PC:

- **Potentiometer** sets control rod position, which the rod worth curve converts to reactivity ρ.
- **Arduino** solves the 6-group point kinetics equations (PKE) plus a lumped temperature feedback model.
- **LEDs** show plant state: subcritical, critical, supercritical, warning and SCRAM.
- **Buzzer** sounds on high power, high temperature or short period.
- **Servo** physically shows rod insertion.
- **SCRAM pushbutton** drives the rods in. The trip latches until reset.
- **Serial Plotter / CSV log** shows power, period, reactivity and temperature in real time.

**Headline claim:** six delayed groups and temperature feedback, solved on the microcontroller with a stiffness-aware implicit integrator, then validated against a SciPy reference solution.

## 2. Physics model

### 2.1 Point kinetics (6 delayed groups) [R1][R2]

$$\frac{dn}{dt}=\frac{\rho-\beta}{\Lambda}n+\sum_{i=1}^{6}\lambda_iC_i+S,\qquad
\frac{dC_i}{dt}=\frac{\beta_i}{\Lambda}n-\lambda_iC_i$$

| Symbol | Meaning | Value / units |
|---|---|---|
| $n$ | Neutron population, normalized as power ($n_0=1$ = 100 % rated) | — |
| $C_i$ | Precursor concentration for group $i$ | same units as $n$ |
| $\rho$ | Reactivity $(k-1)/k$ | 1 pcm = $10^{-5}$; \$1 = $\beta$ |
| $\beta$ | Total delayed neutron fraction, U-235 thermal | 0.0065 [R2][R7] |
| $\Lambda$ | Prompt neutron generation time | $10^{-4}$ s (thermal reactor; DOE gives $10^{-4}$–$10^{-5}$ s) [R2][R7] |
| $S$ | External source (lets you do a startup from shutdown) | $\approx3\times10^{-4}$ s⁻¹, giving shutdown power $\approx10^{-6}$ at −\$5 ($n_{ss}=-S\Lambda/\rho$) |

**Equilibrium initial condition:** $C_{i0}=\beta_i n_0/(\lambda_i\Lambda)$. With $\rho=0$ and $S=0$, $n$ must stay constant. This is test V1 in §6.

### 2.2 Keepin 6-group data for U-235 thermal fission [R3][R4][R7]

| Group | $t_{1/2}$ (s) | $\lambda_i$ (s⁻¹) | Relative abundance | $\beta_i$ |
|---|---|---|---|---|
| 1 | 55.72 | 0.0124 | 0.033 | 0.000215 |
| 2 | 22.72 | 0.0305 | 0.219 | 0.001424 |
| 3 | 6.22 | 0.111 | 0.196 | 0.001274 |
| 4 | 2.30 | 0.301 | 0.395 | 0.002568 |
| 5 | 0.610 | 1.14 | 0.115 | 0.000748 |
| 6 | 0.230 | 3.01 | 0.042 | 0.000273 |
| **Total** | | | 1.000 | **0.0065** |

> **Verified** against DOE-HDBK-1019/1-93, Module NP-02, Table 3 (p. 30) [R7]. DOE lists the same half-lives and fractions rounded to 2–3 figures (e.g. 0.00021 vs 0.000215), total 0.0065. With $\lambda=\ln2/t_{1/2}$, the DOE half-lives give the same $\lambda_i$ except group 3: 6.2 s → 0.112 vs Keepin's 6.22 s → 0.111. Vol. 2 (NP-04 Table 1, p. 12) lists group 3 as 6.22 s, matching Keepin exactly [R5], so keep the Keepin values. DOE's 12.5 s mean precursor decay time is also consistent with $\sum(\beta_i/\lambda_i)/\beta\approx13$ s.

**Simpler option:** use one delayed group with $\beta=0.0065$ and $\lambda_{\text{eff}}\approx0.08$ s⁻¹. Build this first, then upgrade to 6 groups.

### 2.3 Rod position to reactivity: integral rod worth S-curve [R5]

$$\rho_{\text{rod}}(x)=\rho_w\left[\frac{x}{H}-\frac{1}{2\pi}\sin\frac{2\pi x}{H}\right]-\rho_{\text{crit}}$$

- $x/H = \text{pot}/1023$, smoothed with an exponential moving average (EMA).
- Total worth $\rho_w\approx \$2.2$.
- Choose $\rho_{\text{crit}}$ so the reactor is critical at about 50 % withdrawn. Full withdrawal then reaches just past prompt critical, which is the danger demo.
- **Rate-limit rod motion** (for example 2 %/s, like a real rod drive). Then a fast pot twist can't step in a dollar of reactivity instantly. Real plants do this, and so does pyReactor [P2]. DOE says maximum rod speed is limited to soften a continuous-withdrawal accident [R5].
- **The S shape is verified** [R5]: differential worth is bell-shaped (low at the ends, peak mid-core) and integral worth is an S. The sine formula is a smooth fit. Checked against DOE Example 3 data, every point is within 3% of total worth.
- **Add a safety rod bank** worth about \$4, driven by the SCRAM button. DOE separates shim, regulating and safety rods. The regulating rod alone only reaches −\$1.1 (≈ −0.7% Δk/k), below DOE's typical 1–5% shutdown margin. Both banks in give −\$5.1 ≈ −3.3%. Bickel's FSAR example credits about 5% shutdown margin (k ≈ 0.95) [R8]. To match it, raise $\rho_s$ to about \$6.6 (total −\$7.7 ≈ −5%).

### 2.4 Temperature feedback (one lumped node)

$$mc\frac{dT}{dt}=P-h(T-T_{\text{cool}}),\qquad
\rho=\rho_{\text{rod}}+\alpha_T(T-T_{\text{ref}}),\quad\alpha_T<0$$

**Demo-scaled constants** (not real plant values):

| Constant | Value | Note |
|---|---|---|
| $T_{\text{cool}}$ | 290 | coolant temperature |
| $h$ | 1/30 | gives a 30 K rise at $P=1$ |
| $T_{\text{ref}}$ | 320 | equals $T_{\text{cool}}+P_0/h$, so feedback is zero at the starting state |
| $mc$ | 5 | thermal time constant $mc/h\approx150$ s, so it is visible live |
| $\alpha_T$ | −5 pcm/K | Demo value. DOE's worked example uses −8.2 pcm/°F (≈ −15 pcm/K) for a moderator coefficient [R5], so this is the same order of magnitude but gentler |

**Expected result:** a +\$0.2 rod pull (130 pcm) settles about 26 K hotter and at about 1.9× power. This shows the reactor stabilizing itself.

**Checked against Bickel, Module 10 [R8]:**

- **The structure matches.** Bickel's coupled model (slide 16) is $\delta\rho_{\text{net}}=\rho+\frac{\partial\rho}{\partial T_f}\Delta T_f+\frac{\partial\rho}{\partial T_{\text{avg}}}\Delta T_{\text{avg}}$ with a fuel heat balance $\tau_f\,dT_f/dt=P-(T_f-T_{\text{avg}})/R_{f\text{-}c}$. This plan lumps fuel and coolant into one node.
- **Real values vs. this demo:**

| Quantity | Real plant [R8] | This demo |
|---|---|---|
| Doppler (fuel) coefficient | Always negative. Diablo Canyon −1.4 to −2.6 pcm/°F (≈ −2.5 to −4.7 pcm/K) | Lumped into $\alpha_T$ |
| Moderator temperature coefficient | Example −25 pcm/°F (≈ −45 pcm/K). A PWR's MTC can be slightly positive early in core life with high soluble boron | Lumped into $\alpha_T$ |
| MTC typical range | 0 to −80 pcm/°C (= pcm/K). It must be non-positive at significant power (NRC SRP 4.3) [R9][R10] | −5 pcm/K is inside it |
| Total power coefficient | Diablo Canyon −11 to −15 pcm/% power early in life, −20 to −31 late in life | −1.5 pcm/% power |
| Power defect, 0 → 100% | About 2000–2250 pcm (≈ \$3) | 150 pcm (≈ \$0.23) |
| Fuel temperature lag | $\tau_f\approx$ 3–5 s | One node, 150 s |

- **The demo is deliberately about 10× gentler.** A real \$3 power defect is more than the \$2.2 rod worth, so the rod could never bring the reactor to full power. Real PWRs also use soluble boron to cover it. Say this in the report.
- **Something you can show:** at zero power the core is 30 K cooler, which adds +150 pcm. The reactor therefore goes critical at about 45% withdrawn during startup, compared with 50% at full power.
- **Overcooling demo** (use the optional coolant pot as $T_{\text{cool}}$): lower $T_{\text{cool}}$ by 10 K with the rod fixed. The colder core gains reactivity, so power rises until $T$ is back at $T_{\text{ref}}$, at $P=1+10/30\approx1.33$. This is why the MTC has a *lower* limit, typically about −80 pcm/°C: in a main steam line break the coolant cools quickly, and a strongly negative MTC turns that into a large positive reactivity insertion [R9].
- **Expected dynamics** (slide 17): a reactor at power is highly damped. After a small step, power rises, overshoots, then settles where the net $\rho=0$. My rough frequency-response estimate predicted 10–20% overshoot and about 10 min to settle. The SciPy reference and the C++ model both show a much larger first peak. A +\$0.2 pull reaches $n\approx3.4$ about 60 s in, which is ~80% above the final 1.87. Power is then within 2% of its final value by about 9 min, with no sustained ringing. On the real panel, a +\$0.2 pull from full power therefore trips on high power (1.20).
- **Optional upgrade:** use Bickel's two-node model, with a fast fuel node ($\tau_f\approx4$ s, Doppler) and a slow coolant node (MTC).

### 2.5 Behaviors to demonstrate [R2][R6]

| Regime | ρ | Expected behavior |
|---|---|---|
| Subcritical | < 0 | Prompt drop, then decay. The period approaches about −80 s (set by the 55 s group). |
| Critical | 0 | Constant power |
| Supercritical (delayed) | +\$0.1 | Period ≈ 98 s from the exact inhour equation. One group with DOE's power-rising $\lambda_{\text{eff}}=0.1$ gives 90 s [R5] |
| Supercritical (delayed) | +\$0.5 | Prompt jump of about 2×, then period ≈ 5.8 s (exact inhour). One group gives 10–12.5 s, which is too slow |
| Prompt critical | ≥ \$1 | Period about $\Lambda/(\rho-\beta)$, in milliseconds. Auto-trip and clamp $n$. |
| SCRAM | → −\$5 | Immediate drop to $\beta/(\beta-\rho)$ of power, then the ~80 s tail (DOE: about −80 s, −1/3 DPM [R5]) |

**Benchmark:** Lamarsh gives a 57 s period for a 0.1 % δk/k insertion, compared with about 0.1 s with no delayed neutrons [R6]. Use this as a validation point.

**Instrumentation:** startup rate $\text{SUR}=26.06/T$ in decades per minute (DPM), the same unit real consoles use. Here $26.06 = 60/\ln 10$.

## 3. Numerical method

**Problem: the system is stiff.** The fast eigenvalue is about $-(\beta-\rho)/\Lambda\approx-65$ s⁻¹, while the slowest precursor decays at 0.0124 s⁻¹. Explicit Euler is only stable for
$\Delta t<2\Lambda/(\beta-\rho)\approx31$ ms at $\rho=0$ (about 5 ms right after a −\$5 SCRAM), and RK4 only improves the limit by about 1.4×.

**Chosen method: backward (implicit) Euler with closed-form elimination.** It is unconditionally stable, has no matrix solve and costs about 40 floating-point operations (flops) per step.

1. $C_i^{k+1}=\dfrac{C_i^k+h\beta_i n^{k+1}/\Lambda}{1+h\lambda_i}$
2. Substitute into the $n$ equation and solve the scalar:

$$n^{k+1}=\frac{n^k+h\sum_i\dfrac{\lambda_iC_i^k}{1+h\lambda_i}+hS}
{1-\dfrac{h(\rho-\beta)}{\Lambda}-\dfrac{h^2}{\Lambda}\sum_i\dfrac{\lambda_i\beta_i}{1+h\lambda_i}}$$

3. Update $C_i^{k+1}$ with the new $n^{k+1}$.

**Alternatives to document in the report as comparisons:**
- Explicit Euler with sub-steps. This shows the instability; run it on purpose as a demo.
- The prompt jump approximation (PJA), $n=\Lambda\sum\lambda_iC_i/(\beta-\rho)$. It is accurate to about 1 % for $\rho\lesssim0.5\beta$ and singular at \$1 [R2].

**Optional upgrade:** Crank–Nicolson (second order), or step-halving to show convergence.

```cpp
// Backward Euler PKE step — h in seconds, rho in absolute units
const float beta_i[6] = {0.000215,0.001424,0.001274,0.002568,0.000748,0.000273};
const float lam_i[6]  = {0.0124,0.0305,0.111,0.301,1.14,3.01};
const float BETA = 0.0065, LAMBDA = 1e-4;
float n = 1.0, C[6];

void initPrecursors() { for (int i=0;i<6;i++) C[i] = beta_i[i]*n/(lam_i[i]*LAMBDA); }

void pkeStep(float rho, float h, float S) {
  float num = n + h*S, den = 1.0 - h*(rho - BETA)/LAMBDA;
  for (int i=0;i<6;i++) {
    float d = 1.0 + h*lam_i[i];
    num += h*lam_i[i]*C[i]/d;
    den -= h*h*lam_i[i]*beta_i[i]/(LAMBDA*d);
  }
  n = num/den;
  if (n > 1e6) n = 1e6;   // float-overflow guard past prompt critical
  for (int i=0;i<6;i++) C[i] = (C[i] + h*beta_i[i]*n/LAMBDA)/(1.0 + h*lam_i[i]);
}
```

**Period / SUR estimate:** $T=\Delta t/\ln(n_k/n_{k-1})$, smoothed with an EMA. A short-period trip fires at, for example, $|T|<10$ s.

## 4. Hardware

### 4.1 Board choice

| | Uno R3 [H1] | Uno R4 Minima [H2][H3] |
|---|---|---|
| CPU | ATmega328P, 8-bit, 16 MHz, software float | Renesas RA4M1, Arm Cortex-M4, 48 MHz. The FPU is believed present but not verified. |
| RAM | 2 kB | 32 kB |
| Per-pin current | ~20 mA recommended. This is the common figure, not verified against the datasheet. | **8 mA**, so use about 1 kΩ LED resistors |
| Wokwi simulation | Yes | No [H11] |

**Recommendation:** prototype in **Wokwi on R3**, then build on **R4 Minima**. The implicit solver fits on the R3, but the R4 has much more headroom for 6 groups, feedback and display. Measure `micros()` per step on both and report it.

**As built:** the **ELEGOO UNO R4 WiFi** from a starter kit. It has the same RA4M1 chip, 32 kB RAM and 8 mA pin limit as the Minima, so the firmware is unchanged. PlatformIO environment: `uno_r4_wifi` (19 % RAM, 31 % flash).

### 4.2 Bill of materials

Ordered from Amazon. Prices checked 2026-11-21 (Black Friday).

| Item | Qty used | Purpose | Price |
|---|---|---|---|
| [ELEGOO UNO R4 WiFi Super Starter Kit](https://www.amazon.com/dp/B0GV391HB3) | 1 | Board, breadboard, jumpers, LEDs, resistors, buttons, active buzzer, PN2222 transistor, SG90 servo, breadboard power module | $45.99 (list $59.99) |
| [Flutesan 10 kΩ trim pots with knobs, 24-pc](https://www.amazon.com/dp/B09L5MGNFZ) | 2 | Rod and coolant knobs (the kit has only one pot). Linear, 270°, round breadboard pins | $9.99 ($9.49 Prime) |
| [E-Projects 470 µF 50 V electrolytic, 5-pc](https://www.amazon.com/dp/B07YN5SZF4) | 2 | Across the servo supply; two in parallel ≈ 940 µF | $7.18 |
| [ELEGOO 0.96" SSD1306 OLED, I²C, 3-pc](https://www.amazon.com/dp/B0D2RMQQHR) | 1 | Readouts for power, period and temperature (0x3C, 3.3–5 V) | $8.49 |
| *Optional, not ordered:* 10-segment LED bar graph | 1 | **Log-power** display (one segment per decade) | ~$3 |

**Total:** $71.65 before tax ($71.15 with the Prime pot price). Over $35, so shipping is free.

- **LED resistors:** the kit has no 470 Ω. Use 1 kΩ (≈3 mA) or two 220 Ω in series (≈7 mA). Never a single 220 Ω or 330 Ω on an R4 pin.
- **Capacitors:** put them right at the servo supply, long leg (+) to servo 5 V and striped leg (−) to GND. The 50 V rating is fine at 5 V.
- **Servo power:** the kit's breadboard power module or a 4×AA pack, never the Arduino 5 V pin.

### 4.3 Pin map (same on R3 and R4)

| Pin | Function | Reason |
|---|---|---|
| D2 | SCRAM button (`INPUT_PULLUP`, `FALLING` interrupt) | Only D2 and D3 support interrupts [H5] |
| D3 | Reset/acknowledge button | Interrupt-capable |
| D4 | Active buzzer (or D8 with `tone()` for a passive buzzer) | Avoids the Timer2 conflict on D3 and D11 [H8] |
| D5, D6 | Green and yellow LEDs (PWM) | |
| D7, D12 | Red (SCRAM) and blue (critical) LEDs | |
| D9 | Servo signal | The Servo library disables PWM on D9 and D10 [H7] |
| A0 / A1 | Rod potentiometer / coolant potentiometer | 10-bit ADC, 0–1023 [H9] |
| A4 / A5 | SDA / SCL for the OLED or LCD | [H4] |

### 4.4 Electrical design (put this in the report; it is the E&M part of ENGR 217)

- **LED resistor:** $R=(V_{cc}-V_f)/I$.
  - R3: $(5-2)/220\approx13.6$ mA.
  - R4: use $1\text{ kΩ}\to3$ mA to stay under the 8 mA pin limit [H3].
- **Potentiometer:** works as a voltage divider, $V_{\text{wiper}}=5\cdot\text{ADC}/1023$.
- **Servo power:**
  - Use a separate 5 V supply and tie the grounds together [H7]. The R4 manual says never to power servos from the board's 5 V pin [H4].
  - The capacitor absorbs current spikes. Without it, a brownout can reset the board, which looks like a phantom SCRAM.
- **Button:** the ISR only sets a `volatile` flag, because `millis()` doesn't advance inside an ISR [H5]. Debounce for 20–50 ms in the main loop.

## 5. Software architecture

```
loop():
  every DT = 10 ms (millis(), never delay()):
    read pots → EMA → rate-limit rod → rho_rod(x)
    rho = rho_rod + alpha_T*(T - T_ref)       (forced to −$5 if tripped)
    pkeStep(rho, DT, S)  [optionally 2–5 substeps]
    thermal step (backward Euler)
    period/SUR estimate
    trip logic: P > 1.2, T > T_max, |period| < 10 s, manual SCRAM → latch
    outputs: LEDs, buzzer, servo angle = rod %, OLED
  every 100 ms: Serial.print("P:..,rho$:..,T:..,SUR:..")  → Serial Plotter [H10]
```

**State machine:** SHUTDOWN → STARTUP → CRITICAL/POWER → TRIPPED (latched) → RESET (allowed only with rods fully inserted).

**Serial output:** 115200 baud. Use `label:value` pairs separated by commas [H10]. Also add a CSV mode (`t,n,rho,T`) for validation.

## 6. Validation plan

| # | Test | Pass criterion |
|---|---|---|
| V1 | Equilibrium: $\rho=0$ for 10 min | $\lvert n-1\rvert<10^{-4}$ |
| V2 | Step +0.1 % δk/k | Stable period ≈ 57 s (Lamarsh benchmark [R6]) and matches the inhour equation |
| V3 | Steps of +\$0.1, +\$0.5 | Prompt jump ≈ $\beta/(\beta-\rho)$; period within a few % of SciPy |
| V4 | SCRAM from full power | Prompt drop ≈ $\beta/(\beta-\rho)$, then the ~80 s negative period |
| V5 | Reference comparison | Log Arduino CSV and solve the same 7 ODEs with `scipy.integrate.solve_ivp(method="Radau", rtol=1e-8)`. Plot the error. |
| V6 | Step-size sweep | Halve Δt and show first-order convergence |
| V7 | Explicit Euler with Δt > 31 ms (e.g. 40 ms) | Shows the instability on purpose. Good figure for the report. |
| V8 | Temperature feedback | +\$0.2 settles at the predicted ΔT ≈ 26 K with net ρ → 0; first peak ~80% above final (n ≈ 3.4 at 60 s), within 2% by ~9 min, no sustained ringing [R8] |

## 7. Build phases

1. **Desk physics:** write a Python reference solver (SciPy) and the one-group model, and reproduce the §2.5 table.
2. **Wokwi prototype:** wire the potentiometer, LEDs, buzzer, servo and button on a virtual Uno R3, then port the solver [H11][H12].
3. **Six groups + implicit Euler:** run V1–V4 and V6–V7 on the simulated board.
4. **Buy parts and build** on the R4 (ELEGOO UNO R4 WiFi, see §4.2). Measure µs per step.
5. **Feedback + trips + log-power/SUR display:** run V8.
6. **Validation write-up:** V5 plots, error analysis and a demo video.

## 8. Risks

| Risk | Mitigation |
|---|---|
| Explicit integrator blows up | Implicit Euler (§3) |
| Float overflow past prompt critical | Clamp $n$ and auto-trip on short period |
| Servo brownout resets the board | Separate supply, shared ground, bulk capacitor |
| Potentiometer noise looks like reactivity noise | EMA filter plus rod rate limit |
| R4 8 mA pin limit | 1 kΩ LED resistors, or drive LEDs through a transistor |
| Scope creep (Xe/Sm poisons, multiple loops) | Leave these as stretch goals only |

## 9. Prior art and inspiration

| Project | What it is | What to borrow / how this differs |
|---|---|---|
| **DUNE** (H. Kabiraj, fork of pyReactor) [P1] | Python 6-group PKE with fuel/coolant feedback, Xe/Sm poisons and CSV logging. The Arduino only drives a servo and LEDs. | Closest match. **Difference: here the physics runs on the Arduino.** |
| **pyReactor** (W. Gurecky) [P2] | Python simulator plus a LEGO/Arduino reactor | Rate-limited rods, latched SCRAM, temperature shown as margin to the trip setpoint |
| Hackaday on pyReactor [P3] | Article | Good citation for a well-known hobby build |
| **Cornell ECE 4760 Fission Simulator** [P4][P5] | RP2040 RBMK particle model | Dual-core split: one core simulates, the other handles display/audio |
| **NuclearPi** (ex-Westinghouse operator) [P6] | Raspberry Pi panel | Annunciator-style alarm layout |
| **RBMK control panel replica** [P7] | ESP32 panel | Event log or printout adds authenticity |
| ESP32_BWR_sim; Nuclear-Reactor-System-Control-Room [P8] | MCU reactor sims | GitHub search listings only, not reviewed |
| **openpointkinetics** (R. Morrison) [P9] | Python teaching solver (GPL-3.0), based on Duderstadt & Hamilton | Use as a second reference for V5 |
| Other PKE solvers (tpke, EPKE, …) [P10] | Fortran / C++ / MATLAB | EPKE is a stiffness-aware method that suits a microcontroller |
| **Purdue PUR-1** [P11] | 10 kW research reactor, the only NRC-licensed reactor with fully digital instrumentation & control (I&C) | Real-world motivation for digital I&C |

**Real console features to imitate.** These are standard instrumentation concepts. Before citing them, confirm with the DOE Fundamentals Handbook [R5] or NRC training material.

- Source, intermediate and power range readouts
- **Log power** display
- **SUR meter in DPM** and a period meter
- Rod position readout
- High-power, short-period and manual trips
- Latched annunciators that need an acknowledge
- Rod-withdrawal interlock during a short period

**What makes this stand out:**
1. The solver runs on the board, not on a PC.
2. Six groups with an implicit integrator, plus a reported timing budget.
3. Log-power and SUR display.
4. Short-period trip.
5. Prompt drop and inhour checks against theory.
6. Quantified error against SciPy.

## 10. Citations

**Physics**
- [R1] Wikipedia, "Nuclear reactor physics" (Kinetics section; cites Lamarsh & Baratta p. 329, Lewis pp. 123–126). https://en.wikipedia.org/wiki/Nuclear_reactor_physics
- [R2] Wikipedia, "Delayed neutron" (β = 0.0065, group half-lives, prompt jump). https://en.wikipedia.org/wiki/Delayed_neutron
- [R3] G. R. Keepin, T. F. Wimett, R. K. Zeigler, "Delayed Neutrons from Fissionable Isotopes of Uranium, Plutonium and Thorium," *Phys. Rev.* 107, 1044 (1957). Also Keepin, *Physics of Nuclear Kinetics*, Addison-Wesley, 1965.
- [R4] J. R. Lamarsh & A. J. Baratta, *Introduction to Nuclear Engineering*, 3rd ed., Pearson. Also J. J. Duderstadt & L. J. Hamilton, *Nuclear Reactor Analysis*, Wiley, 1976, ch. 6.
- [R5] U.S. DOE, *DOE Fundamentals Handbook: Nuclear Physics and Reactor Theory*, Vol. 2 of 2, DOE-HDBK-1019/2-93. Module NP-03 covers reactivity, coefficients and control rods (pp. 17–58). Module NP-04 covers subcritical multiplication, kinetics and operation (pp. 1–34). Checked from a PDF copy, 2026-11-20. https://web.archive.org/web/20170914172136/https://energy.gov/sites/prod/files/2013/06/f2/h1019v2.pdf
- [R6] Wikipedia, "Prompt criticality" (57 s vs 0.1 s period; SL-1). https://en.wikipedia.org/wiki/Prompt_criticality
- IAEA Nuclear Data for Safeguards, Table A-6 (U-235 ν_d). https://www-nds.iaea.org/sgnucdat/a6.htm
- [R7] U.S. DOE, *DOE Fundamentals Handbook: Nuclear Physics and Reactor Theory*, Vol. 1 of 2, DOE-HDBK-1019/1-93, Module NP-02, "Prompt and Delayed Neutrons," Table 3, pp. 29–30. https://www.energy.gov/sites/default/files/2026-04/DOE-HDBK-1019-93_VOL1.pdf
- [R8] J. H. Bickel, *Fundamentals of Nuclear Engineering*, Module 10, "Power Reactor Feedback Effects," Rev. 01 (lecture slides; NRC ADAMS ML12142A130). Plant data quoted from the Diablo Canyon (PWR) and Dresden (BWR) FSARs. Checked from a PDF copy, 2026-11-20. https://www.nrc.gov/docs/ML1214/ML12142A130.pdf
- [R9] nuclear-power.com, "Moderator Temperature Coefficient – MTC" (secondary educational site; typical range 0 to −80 pcm/°C, lower-limit reasoning, worked example −30 pcm/°C × 20 °C = −600 pcm). https://www.nuclear-power.com/nuclear-power/reactor-physics/nuclear-fission-chain-reaction/reactivity-coefficients-reactivity-feedbacks/moderator-temperature-coefficient-mtc/
- [R10] U.S. NRC, NUREG-0800 *Standard Review Plan*, Chapter 4, Section 4.3 "Nuclear Design," Rev. 3 (03/2007). Index page checked 2026-11-20. https://www.nrc.gov/reading-rm/doc-collections/nuregs/staff/sr0800/ch4/

**Hardware**
- [H1] https://docs.arduino.cc/hardware/uno-rev3/
- [H2] https://docs.arduino.cc/hardware/uno-r4-minima/
- [H3] https://store.arduino.cc/products/uno-r4-minima
- [H4] https://docs.arduino.cc/tutorials/uno-r4-minima/cheat-sheet/
- [H5] https://docs.arduino.cc/language-reference/en/functions/external-interrupts/attachInterrupt/
- [H6] https://www.adafruit.com/product/169
- [H7] https://docs.arduino.cc/libraries/servo/
- [H8] https://docs.arduino.cc/language-reference/en/functions/advanced-io/tone/
- [H9] https://docs.arduino.cc/learn/microcontrollers/analog-input/
- [H10] https://docs.arduino.cc/software/ide-v2/tutorials/ide-v2-serial-plotter/
- [H11] https://docs.wokwi.com/getting-started/supported-hardware
- [H12] https://wokwi.com/arduino

**Prior art**
- [P1] https://github.com/approx-infinity/DUNE
- [P2] https://github.com/wgurecky/pyReactor
- [P3] https://hackaday.com/2016/02/22/lego-nuclear-reactor-uses-arduino/
- [P4] https://ece4760.github.io/Projects/Spring2025/ks875_ttw24_3rdGroupMember/index.html
- [P5] https://hackaday.com/2025/06/19/fission-simulator-melts-down-rp2040/
- [P6] https://hackaday.com/2019/06/27/nuclear-reactor-simulator-is-the-project-of-a-lifetime/
- [P7] https://hackaday.com/2026/02/11/making-a-functional-control-panel-of-the-chernobyl-rbmk-reactor/
- [P8] https://github.com/search?q=arduino+reactor+simulator&type=repositories
- [P9] https://github.com/richmorrison/openpointkinetics
- [P10] https://github.com/search?q=point+kinetics&type=repositories
- [P11] https://engineering.purdue.edu/NE/research/facilities/reactor

## 11. Not yet verified — check before citing in a report

- Bickel's plant coefficients [R8] come from two specific FSARs (Diablo Canyon, Dresden). Quote them as examples, not as typical values for every reactor.
- The "MTC non-positive at significant power" wording is quoted by [R9]. Before quoting it, read it in the SRP 4.3 PDF itself [R10]. The 0 to −80 pcm/°C range is from [R9] only, which cites no source for it.
- The exact sine form of the rod-worth curve (the S shape is verified [R5]; the formula is a fit).
- The FPU on the RA4M1.
- The ATmega328P 20/40 mA pin limits.
- SG90 stall current and AVR float timing (measure these yourself).
- US part prices.
- These sources failed to load: the IAEA simulator page, the PULSTAR, Breazeale and Texas A&M NSCR pages, and the NRC training PDFs.
- No published paper on running PKE on a microcontroller or FPGA was found. Search IEEE Xplore and ANS Transactions through the TAMU library.
