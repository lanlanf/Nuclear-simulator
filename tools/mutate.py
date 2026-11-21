"""mutate.py - repeatable mutation check for the unit tests (TEST-05).

Applies each mutation from .planning/reviews/REVIEW-tests.md one at a time,
rebuilds, runs build/unit_tests.exe and restores the source. A mutant is
killed if the build or the tests fail. Exit 0 if survivors <= --max-survivors,
1 if more survive, 2 if a mutation no longer matches the source exactly once.

  python tools/mutate.py [--only M01,M31] [--max-survivors 8]
"""
import argparse
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = "lib/reactor/src/"
TESTS = os.path.join(ROOT, "build", "unit_tests.exe")

R, PR, PL, K, I, P = (SRC + f for f in
                      ("reactor.cpp", "protection.cpp", "plant.cpp", "kinetics.cpp",
                       "instruments.cpp", "params.h"))

MUTATIONS = [  # id, file, old, new
    ("M01", R, " + feedback(dT_, pp_)", ""),
    ("M02", R, "  pp_.Tcool = Tcool;\n", ""),
    ("M03", PR, "  if (T > s.tempTrip) trips |= TRIP_HIGH_TEMP;\n", ""),
    ("M04", PR, "  if (T > s.tempWarn) warn |= WARN_TEMP;\n", ""),
    ("M05", PR, "  if (period > 0.0f && period < s.periodWarn) warn |= WARN_PERIOD;\n", ""),
    ("M06", PR, "if (period > 0.0f && period < s.periodWarn)", "if (period < s.periodWarn)"),
    ("M07", PR, "if (rodPos >= 0.01f || present != 0) return false;",
     "if (rodPos >= 0.01f) return false;"),
    ("M08", I, "return 26.06f / period;", "return 26.06f / fabsf(period);"),
    ("M09", R, "safety_ = moveRod(safety_, 0.0f, pp_.scramSpeed, h);",
     "safety_ = moveRod(safety_, 0.0f, pp_.rodSpeed, h);"),
    ("M10", R, "safety_ = moveRod(safety_, 1.0f, pp_.rodSpeed, h);",
     "safety_ = moveRod(safety_, 1.0f, pp_.scramSpeed, h);"),
    ("M11", R, "TRIP_HIGH_POWER | TRIP_HIGH_TEMP | TRIP_SHORT_PERIOD;",
     "TRIP_HIGH_POWER | TRIP_SHORT_PERIOD;"),
    ("M12", R, "TRIP_HIGH_POWER | TRIP_HIGH_TEMP | TRIP_SHORT_PERIOD;",
     "TRIP_HIGH_POWER | TRIP_HIGH_TEMP | TRIP_SHORT_PERIOD | TRIP_FAULT;"),
    ("M13", R, "if (!ok || inputFault) present", "if (inputFault) present"),
    ("M14", R, "r_.t = static_cast<float>(tWhole_) + tFrac_;", "r_.t += h;"),
    ("M15", PL, "  if (demand < 0.0f) demand = 0.0f;\n", ""),
    ("M16", PL, "  if (demand > 1.0f) demand = 1.0f;\n", ""),
    ("M17", PL, "p.alphaT <= 0.0f && p.MC > 0.0f && p.K > 0.0f", "p.alphaT <= 0.0f"),
    ("M18", PL, "p.alphaT <= 0.0f", "p.alphaT < 0.0f"),
    ("M19", I, "return log10f(n > N_MIN ? n : N_MIN);", "return log10f(n);"),
    ("M20", I, "OMEGA_MIN = 1.0e-4f;", "OMEGA_MIN = 1.0e-2f;"),
    ("M21", I, "if (m.acc + 0.5f * h < SAMPLE_DT) return;", "if (m.acc < SAMPLE_DT) return;"),
    ("M22", I, "    if (m.count < PERIOD_SAMPLES) return;\n", ""),
    ("M23", I, "EMA = 0.2f;", "EMA = 1.0f;"),
    ("M24", PL, "const float maxStep = speed * h;", "const float maxStep = 0.5f * speed * h;"),
    ("M25", P, "p.MC = 5.0f;", "p.MC = 1.5f;"),
    ("M26", K, "rho >= beta ? 10 : 1;", "1;"),
    ("M27", PR, "if (rhoDollars >= 1.0f)", "if (rhoDollars > 1.05f)"),
    ("M28", P, "Setpoints s = {1.20f,", "Setpoints s = {1.50f,"),
    ("M29", R, "rod_ > rodDemand ? rod_ : rodDemand", "rod_"),
    ("M30", R, "if (!isValid(plant)) return false;",
     "if (!isValid(plant)) {\n    latched_ = 0;\n    return false;\n  }"),
    ("M31", R, " + feedback(dT_, pp_)", " - feedback(dT_, pp_)"),
    ("M32", K, "float num = k.n + hs * S;", "float num = k.n;"),
]


def read(rel, old, new):
    """Source text plus old/new adjusted to the file's line endings."""
    with open(os.path.join(ROOT, rel), newline="") as f:
        text = f.read()
    if "\r\n" in text:
        old, new = old.replace("\n", "\r\n"), new.replace("\n", "\r\n")
    return text, old, new


def build():
    return subprocess.run(["cmake", "--build", "build"], cwd=ROOT, capture_output=True).returncode


def run_one(m):
    _, rel, old, new = m
    path = os.path.join(ROOT, rel)
    original, old, new = read(rel, old, new)
    try:
        with open(path, "w", newline="") as f:
            f.write(original.replace(old, new, 1))
        if build() != 0:
            return "build-fail"
        tests = subprocess.run([TESTS], cwd=ROOT, capture_output=True)
        return "killed" if tests.returncode != 0 else "survived"
    finally:
        with open(path, "w", newline="") as f:
            f.write(original)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--only", help="comma-separated mutation ids, e.g. M01,M31")
    ap.add_argument("--max-survivors", type=int, default=8)
    args = ap.parse_args()

    chosen = MUTATIONS
    if args.only:
        ids = set(args.only.split(","))
        chosen = [m for m in MUTATIONS if m[0] in ids]

    for mid, rel, old, new in chosen:
        text, old, _ = read(rel, old, new)
        count = text.count(old)
        if count != 1:
            print(f"{mid}: expected 1 match in {rel}, found {count}", file=sys.stderr)
            return 2

    survivors = []
    try:
        for m in chosen:
            result = run_one(m)
            print(f"{m[0]:<5}{m[1].removeprefix(SRC):<28}{result:>10}", flush=True)
            if result == "survived":
                survivors.append(m[0])
    finally:
        build()

    print(f"survivors: {len(survivors)}/{len(chosen)}" + (f" ({', '.join(survivors)})" if survivors else ""))
    return 0 if len(survivors) <= args.max_survivors else 1


if __name__ == "__main__":
    sys.exit(main())
