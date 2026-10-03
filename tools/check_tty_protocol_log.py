#!/usr/bin/env python3
"""Validate the last complete native TTY protocol run, not Shell echo status."""
import argparse
from pathlib import Path

BEGIN = "TTY PROTOCOL GREEN BEGIN"
END = "TTY PROTOCOL GREEN END"
REQUIRED = (
    "TTY CONTROL WAKE PASS:",
    "TTY CONTROLS PASS:",
    "TTY PROTOCOL CONTROLS PASS",
    "TTY CHILD PROTOCOL PASS:",
    "TTY CHILD CYCLE PASS: cycle=1 return=0",
    "TTY CHILD PROTOCOL PASS:",
    "TTY CHILD CYCLE PASS: cycle=2 return=0",
    "TTY CHILD PASS:",
    "TTY PROTOCOL CHILD PASS",
    "TTY WAIT TIMEOUT PASS:",
    "TTY WAIT PASS:",
    "TTY PROTOCOL WAIT PASS",
    "TTY WORKER PASS: cycles=32 workers=64",
    "TTY PROTOCOL WORKER PASS",
    "TTY DOS CHILD PASS:",
    "TTY DOS PASS:",
    "TTY PROTOCOL PACKETS PASS",
)


def validate(text):
    if len(text) > 1024 * 1024:
        raise ValueError("oversized log")
    start = text.rfind(BEGIN)
    if start < 0:
        raise ValueError("missing run")
    lines = text[start:].splitlines()
    if not lines or lines[-1] != END:
        raise ValueError("incomplete run or unexpected trailing output")
    body = lines[1:-1]
    if len(body) != len(REQUIRED):
        raise ValueError("unexpected native output count")
    for line, prefix in zip(body, REQUIRED):
        if not line.startswith(prefix) or "FAIL" in line:
            raise ValueError("missing or reordered native evidence: " + prefix)
    return "TTY PROTOCOL LOG PASS: native controls/wakeup, two children, waits, 64 workers, packets"


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    args = parser.parse_args()
    try:
        print(validate(args.log.read_text(encoding="utf-8")))
    except (OSError, ValueError) as error:
        parser.exit(1, f"TTY PROTOCOL LOG FAIL: {error}\n")
