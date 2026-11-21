"""capture_serial.py - logs the board's CSV output to results/board_<time>.csv.

Puts the board in CSV mode ("m c"), then records for --seconds.

  python tools/capture_serial.py --port COM5 --seconds 120
"""
import argparse
import os
import time

import serial

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HEADER = "t,rho,n,T,period,sur,rod,trip"


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", required=True, help="e.g. COM5 or /dev/ttyACM0")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--seconds", type=float, default=60.0)
    ap.add_argument("--command", action="append", default=[],
                    help="extra serial command sent after 'm c', e.g. --command 'r 0.1'")
    args = ap.parse_args()

    os.makedirs(os.path.join(ROOT, "results"), exist_ok=True)
    path = os.path.join(ROOT, "results", time.strftime("board_%Y%m%d_%H%M%S.csv"))
    with serial.Serial(args.port, args.baud, timeout=1.0) as port, open(path, "w", newline="\n") as out:
        time.sleep(2.0)  # opening the port may reset the board
        port.reset_input_buffer()
        for cmd in ["m c"] + args.command:
            port.write((cmd + "\n").encode())
        out.write(HEADER + "\n")
        rows = 0
        end = time.monotonic() + args.seconds
        while time.monotonic() < end:
            line = port.readline().decode(errors="replace").strip()
            if line[:1].isdigit():  # data rows only, skip the header and messages
                out.write(line + "\n")
                rows += 1
    print(f"wrote {rows} rows to {path}")


if __name__ == "__main__":
    main()
