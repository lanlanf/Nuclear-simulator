"""compare.py - checks sim_cli (float, backward Euler) against the SciPy reference.

Writes figures/overlay_*.png, figures/convergence.png and figures/stability.png,
and prints the error table. Build sim_cli first (cmake --build build).

  python tools/compare.py [--sim build/sim_cli.exe]
"""
import argparse
import io
import os
import subprocess
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
import numpy as np  # noqa: E402

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import reference  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FIGURES = os.path.join(ROOT, "figures")

CASES = [  # scenario, $, t_end
    ("step", 0.1, 300.0),
    ("step", 0.5, 60.0),
    ("step", -5.0, 300.0),
    ("ramp", 0.5, 60.0),
    ("rodpull", 0.2, 1800.0),
]


def run_sim(sim, scenario, rho, t_end, dt=0.01, method="implicit"):
    cmd = [sim, "--scenario", scenario, "--rho-dollars", str(rho), "--t-end", str(t_end),
           "--dt", str(dt), "--method", method]
    out = subprocess.run(cmd, check=True, capture_output=True, text=True).stdout
    return np.genfromtxt(io.StringIO(out), delimiter=",", names=True)


def rel_error(sim, ref):
    n_ref = np.interp(sim["t"], ref["t"], ref["n"])
    return np.abs(sim["n"] - n_ref) / n_ref


def overlays(sim_path):
    print(f"{'case':<18}{'max rel error in n':>20}")
    for scenario, rho, t_end in CASES:
        sim = run_sim(sim_path, scenario, rho, t_end)
        ref = reference.simulate(scenario, rho, t_end)
        err = rel_error(sim, ref)
        name = f"{scenario} {rho:+g}$"
        print(f"{name:<18}{err.max():>20.2e}")

        fig, (ax1, ax2) = plt.subplots(2, 1, sharex=True, figsize=(7, 6))
        ax1.semilogy(ref["t"], ref["n"], "k-", lw=2, label="SciPy Radau (reference)")
        ax1.semilogy(sim["t"], sim["n"], "r--", lw=1.2, label="sim_cli, backward Euler, h = 10 ms")
        ax1.set_ylabel("power n (1 = 100 %)")
        ax1.set_title(f"{name}: simulator vs reference")
        ax1.legend()
        ax1.grid(True, which="both", alpha=0.3)
        ax2.semilogy(sim["t"][1:], np.maximum(err[1:], 1e-12))
        ax2.set_xlabel("time (s)")
        ax2.set_ylabel("relative error")
        ax2.grid(True, which="both", alpha=0.3)
        fig.tight_layout()
        fig.savefig(os.path.join(FIGURES, f"overlay_{scenario}_{rho:+g}.png".replace("+", "p").replace("-", "m")), dpi=150)
        plt.close(fig)


def convergence(sim_path):
    rho, t_end = 0.5, 20.0
    ref = reference.simulate("step", rho, t_end)
    dts = np.array([0.02, 0.01, 0.005, 0.0025, 0.00125])
    errs = np.array([rel_error(run_sim(sim_path, "step", rho, t_end, dt), ref).max() for dt in dts])
    slope = np.polyfit(np.log(dts), np.log(errs), 1)[0]
    print(f"convergence slope {slope:.2f} (1 = first order)")

    fig, ax = plt.subplots(figsize=(6, 4.5))
    ax.loglog(dts * 1000, errs, "o-", label=f"backward Euler (slope {slope:.2f})")
    ax.loglog(dts * 1000, errs[0] * dts / dts[0], "k:", label="first order")
    ax.set_xlabel("time step h (ms)")
    ax.set_ylabel("max relative error in n")
    ax.set_title("+$0.5 step, 20 s")
    ax.legend()
    ax.grid(True, which="both", alpha=0.3)
    fig.tight_layout()
    fig.savefig(os.path.join(FIGURES, "convergence.png"), dpi=150)
    plt.close(fig)


def stability(sim_path):
    rho, t_end = 0.1, 10.0
    ref = reference.simulate("step", rho, t_end)
    fig, ax = plt.subplots(figsize=(7, 4.5))
    ax.plot(ref["t"], ref["n"], "k-", lw=2, label="reference")
    for dt, method, style in [(0.02, "explicit", "b--"), (0.04, "explicit", "r-"), (0.04, "implicit", "g--")]:
        sim = run_sim(sim_path, "step", rho, t_end, dt, method)
        ax.plot(sim["t"], sim["n"], style, label=f"{method} Euler, h = {dt * 1000:g} ms")
    ax.set_ylim(0, 2)
    ax.set_xlabel("time (s)")
    ax.set_ylabel("power n")
    ax.set_title("+$0.1 step: explicit Euler is unstable above h = 2Λ/(β-ρ) ≈ 34 ms")
    ax.legend()
    ax.grid(True, alpha=0.3)
    fig.tight_layout()
    fig.savefig(os.path.join(FIGURES, "stability.png"), dpi=150)
    plt.close(fig)


def main():
    default_sim = os.path.join(ROOT, "build", "sim_cli.exe" if os.name == "nt" else "sim_cli")
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--sim", default=default_sim, help="path to sim_cli")
    args = ap.parse_args()
    os.makedirs(FIGURES, exist_ok=True)
    overlays(args.sim)
    convergence(args.sim)
    stability(args.sim)


if __name__ == "__main__":
    main()
