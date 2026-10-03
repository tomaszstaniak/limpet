#!/usr/bin/env python3
"""Validate every cycle of the repeated native PTY stabilization suite."""
import argparse
from pathlib import Path
try:
    from .check_tty_protocol_log import BEGIN, END, validate as protocol_validate
except ImportError:
    from check_tty_protocol_log import BEGIN, END, validate as protocol_validate

SERVICE = (
    "TTY ZERO FIFO PASS:",
    "TTY DUPLEX PRESSURE PASS: schedules=8 bytes_per_stream=1048568 short_writes=15495 events=417154",
    "TTY ZERO IO PASS:",
    "TTY SERVICE LIFETIME PASS: cycles=1000 temporary EOF, cancellation, final collection",
    "TTY SERVICE PASS:",
)


def validate(text, cycles=10):
    if not 1 <= cycles <= 100 or len(text) > 1024 * 1024:
        raise ValueError("invalid cycle count or oversized log")
    marker = f"TTY STABILIZE BEGIN cycles={cycles}"
    lines = text.splitlines()
    # A historical RED preamble is allowed; a second suite is not silently
    # substituted for an incomplete first one in the same artifact.
    if lines.count(marker) != 1:
        raise ValueError("missing or duplicated suite start")
    lines = lines[lines.index(marker)+1:]
    position = 0
    for cycle in range(1, cycles+1):
        expected = (f"TTY STABILIZE CYCLE {cycle} BEGIN", *SERVICE)
        for index, prefix in enumerate(expected):
            if position >= len(lines):
                raise ValueError("truncated cycle")
            line = lines[position]
            exact = index in (0, 2, 4)
            if (line != prefix if exact else not line.startswith(prefix)) or "FAIL" in line:
                raise ValueError(f"missing cycle {cycle} evidence: {prefix}")
            position += 1
        if position >= len(lines) or lines[position] != BEGIN:
            raise ValueError("missing protocol suite")
        try:
            finish = lines.index(END, position)
        except ValueError:
            raise ValueError("truncated protocol suite") from None
        block = lines[position:finish+1]
        if block.count(BEGIN) != 1:
            raise ValueError("nested protocol restart within cycle")
        protocol_validate("\n".join(block))
        position = finish+1
        if position >= len(lines) or lines[position] != f"TTY STABILIZE CYCLE {cycle} END":
            raise ValueError("missing cycle end")
        position += 1
    if lines[position:] != [f"TTY STABILIZE END cycles={cycles}"]:
        raise ValueError("missing suite end or unexpected trailing output")
    return cycles


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("--cycles", type=int, default=10)
    args = parser.parse_args()
    try:
        with args.log.open("r", encoding="utf-8") as source:
            count = validate(source.read(1024 * 1024 + 1), args.cycles)
        print(f"TTY STABILIZATION LOG PASS: cycles={count} all native suites present")
    except (OSError, ValueError) as error:
        parser.exit(1, f"TTY STABILIZATION LOG FAIL: {error}\n")
