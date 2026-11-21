"""reference.py - SciPy reference solution for the reactor model.

Units: t s, n normalized power (1 = 100 %), rho dk/k internally and $ in the CSV,
T K, period s, SUR decades per minute.
Keep the constants in sync with lib/reactor/src/params.h.

  python tools/reference.py --scenario step --rho-dollars 0.5 --t-end 60 > results/ref_step.csv
  python tools/reference.py --inhour 0.1      # stable period for +$0.1
"""
import argparse
import sys

import numpy as np
from scipy.integrate import solve_ivp
from scipy.optimize import brentq

# Keepin six-group U-235 (physical beta, not beta_eff)
BETA_I = np.array([0.000215, 0.001424, 0.001274, 0.002568, 0.000748, 0.000273])
LAMBDA_I = np.array([0.0124, 0.0305, 0.111, 0.301, 1.14, 3.01])
GEN_TIME = 1.0e-4
BETA = BETA_I.sum()

DOLLAR = 0.0065
RHO_W = 9.0 * DOLLAR
RHO_CRIT = 0.5 * RHO_W
ROD_SPEED = 0.0049
MC = 0.25
K = 1.0 / 30.0
T_COOL = 290.0
T_REF = 320.0
ALPHA_T = -7.5e-4
SOURCE = 3.0e-4
PERIOD_INF = 1.0e9

SCENARIOS = ("hold", "step", "ramp", "rodpull")
COLUMNS = "t,rho,n,T,period,sur,rod,trip"


def inhour_root(rho):
    """Stable period (s) for reactivity rho (dk/k) from the six-group inhour equation."""
    def f(omega):
        return omega * GEN_TIME + np.sum(BETA_I * omega / (omega + LAMBDA_I)) - rho

    if rho > 0:
        return 1.0 / brentq(f, 1.0 / 1.0e7, 1.0 / 1.0e-6, xtol=1e-15, rtol=1e-14)
    if rho < 0:  # slowest decaying root lies between -lambda_1 and 0
        return 1.0 / brentq(f, -LAMBDA_I[0] * (1 - 1e-12), -1e-15, xtol=1e-18, rtol=1e-14)
    return np.inf


def rod_worth(x):
    return RHO_W * (x - np.sin(2 * np.pi * x) / (2 * np.pi)) - RHO_CRIT


def rod_for(rho):
    """Regulating rod position in [0.5, 1] that adds rho."""
    return brentq(lambda x: rod_worth(x) - rho, 0.5, 1.0, xtol=1e-12)


def simulate(scenario, rho_dollars, t_end, dt_out=0.1):
    """Returns a dict of arrays with the CSV columns (rho in $)."""
    target = 0.0 if scenario == "hold" else rho_dollars * BETA
    plant = scenario == "rodpull"
    x_target = rod_for(rho_dollars * BETA) if plant else 0.5

    def rod(t):
        return min(0.5 + ROD_SPEED * t, x_target) if plant else 0.5

    def rho_of(t, dT):
        if plant:
            return rod_worth(rod(t)) + ALPHA_T * dT
        if scenario == "ramp":
            return target * min(t / 10.0, 1.0)
        return target

    def rhs(t, y):
        n, c, dT = y[0], y[1:7], y[7]
        rho = rho_of(t, dT)
        s = SOURCE if plant else 0.0
        dn = (rho - BETA) / GEN_TIME * n + np.dot(LAMBDA_I, c) + s
        dc = BETA_I * n / GEN_TIME - LAMBDA_I * c
        ddT = (n - K * (dT + T_REF - T_COOL)) / MC if plant else 0.0
        return np.concatenate(([dn], dc, [ddT]))

    y0 = np.concatenate(([1.0], BETA_I / (LAMBDA_I * GEN_TIME), [0.0]))
    t_eval = np.round(np.arange(0.0, t_end + 1e-9, dt_out), 10)
    sol = solve_ivp(rhs, (0.0, t_end), y0, method="Radau", t_eval=t_eval,
                    rtol=1e-8, atol=1e-12, max_step=0.5)
    if not sol.success:
        raise RuntimeError(sol.message)

    n, dT = sol.y[0], sol.y[7]
    rho = np.array([rho_of(t, d) for t, d in zip(sol.t, dT)])
    dn = np.array([rhs(t, y)[0] for t, y in zip(sol.t, sol.y.T)])
    omega = dn / n
    period = np.where(np.abs(omega) < 1e-4, PERIOD_INF, 1.0 / np.where(omega == 0, 1, omega))
    return {
        "t": sol.t, "rho": rho / BETA, "n": n, "T": T_REF + dT, "period": period,
        "sur": 26.06 / period, "rod": np.array([rod(t) for t in sol.t]), "trip": np.zeros_like(n),
    }


def write_csv(data, out):
    out.write(COLUMNS + "\n")
    cols = COLUMNS.split(",")
    for i in range(len(data["t"])):
        out.write(",".join(f"{data[c][i]:.10g}" for c in cols) + "\n")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--scenario", choices=SCENARIOS)
    ap.add_argument("--rho-dollars", type=float)
    ap.add_argument("--t-end", type=float, default=300.0)
    ap.add_argument("--inhour", type=float, metavar="DOLLARS", help="print the inhour period and exit")
    args = ap.parse_args()

    if args.inhour is not None:
        print(f"{inhour_root(args.inhour * BETA):.6g}")
        return
    if args.scenario is None:
        ap.error("--scenario or --inhour is required")
    rho = args.rho_dollars
    if rho is None:
        rho = {"ramp": 0.5, "rodpull": 0.2}.get(args.scenario, 0.1)
    write_csv(simulate(args.scenario, rho, args.t_end), sys.stdout)


if __name__ == "__main__":
    main()
