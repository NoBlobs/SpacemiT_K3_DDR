#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Thalles Campagnani
"""Capture a serial console with host timestamps until a pattern or timeout.

usage: serial_log.py [--port /dev/ttyUSB1] [--baud 115200] [--timeout 60]
                     [--until REGEX] [--idle SECONDS] out.log
"""
import argparse
import re
import sys
import time

import serial


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="/dev/ttyUSB1")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--timeout", type=float, default=60.0)
    ap.add_argument("--idle", type=float, default=0.0,
                    help="stop after this many seconds without output (0 = off)")
    ap.add_argument("--until", default=None, help="stop after a line matching REGEX")
    ap.add_argument("out")
    a = ap.parse_args()

    pat = re.compile(a.until) if a.until else None
    t0 = time.monotonic()
    last = t0
    buf = b""
    with serial.Serial(a.port, a.baud, timeout=0.1) as s, open(a.out, "w") as f:
        while True:
            now = time.monotonic()
            if now - t0 > a.timeout:
                f.write(f"[{now - t0:10.6f}] ## serial_log: timeout\n")
                break
            if a.idle and buf == b"" and now - last > a.idle and last != t0:
                f.write(f"[{now - t0:10.6f}] ## serial_log: idle\n")
                break
            chunk = s.read(4096)
            if not chunk:
                continue
            last = now
            buf += chunk
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                text = line.rstrip(b"\r").decode("utf-8", "replace")
                f.write(f"[{now - t0:10.6f}] {text}\n")
                f.flush()
                sys.stdout.write(text + "\n")
                if pat and pat.search(text):
                    return


if __name__ == "__main__":
    main()
