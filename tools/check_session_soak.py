"""Validate strict native session logs, bounded to 8 MiB of UTF-8 input.

Usage: python3 tools/check_session_soak.py LOG [--expected 100]
Total duration includes reporting outside iterations, so it must cover the
first-start to last-end interval, rather than equal the sum of iterations.
"""
import argparse
from pathlib import Path
import re
import sys

MAX_BYTES = 8 * 1024 * 1024
NUMBER = r"([0-9]{1,20})"
PREFIX = r"SESSION ITERATION " + NUMBER + "/" + NUMBER
BEGIN = re.compile(PREFIX + r" BEGIN require_break=1 start_ticks=" + NUMBER)
DISPOSED = re.compile(PREFIX + r" DISPOSED " + " ".join(
    name + "=0" for name in (
        "handles", "locks", "pending_read", "pending_write", "pending_wait",
        "hook", "hook_target", "port", "timer", "startup_handles", "references")))
PASS = re.compile(PREFIX + r" PASS end_ticks=" + NUMBER +
                  r" whole_suite_ticks=" + NUMBER + r" ticks_per_second=50")
COMPLETE = re.compile(r"SESSION COMPLETE completed=" + NUMBER +
                      r" requested=" + NUMBER + r" total_whole_suite_ticks=" +
                      NUMBER + r" ticks_per_second=50")
FAILURE = re.compile(r"\b(?:SESSION FAILED|deadline|assertion|SKIP)\b", re.IGNORECASE)


def check_log(text, expected=100):
    if not isinstance(expected, int) or not 1 <= expected <= 1000:
        raise ValueError("expected count must be 1 through 1000")
    if len(text) > MAX_BYTES or len(text.encode("utf-8")) > MAX_BYTES:
        raise ValueError("log exceeds 8 MiB")
    if not text.endswith("\n"):
        raise ValueError("log is empty or ends with a truncated line")
    iteration, phase = 1, "BEGIN"
    first_start = start = last_end = None
    result = None
    for line_number, line in enumerate(text.splitlines(), 1):
        if FAILURE.search(line):
            raise ValueError(f"failure/skip marker on line {line_number}")
        if "SESSION " not in line:
            continue
        if result is not None:
            raise ValueError(f"unexpected marker after completion on line {line_number}")
        if line.startswith("SESSION COMPLETE"):
            match = COMPLETE.fullmatch(line)
            if not match or phase != "BEGIN" or iteration != expected + 1:
                raise ValueError(f"premature or malformed completion on line {line_number}")
            completed, requested, total = map(int, match.groups())
            if completed != expected or requested != expected or total < last_end - first_start:
                raise ValueError("completion count or total duration is inconsistent")
            result = (completed, total, 50)
            continue
        pattern = {"BEGIN": BEGIN, "DISPOSED": DISPOSED, "PASS": PASS}[phase]
        match = pattern.fullmatch(line)
        if not match or iteration > expected:
            raise ValueError(f"expected iteration {iteration} {phase} on line {line_number}")
        number, requested, *values = map(int, match.groups())
        if number != iteration or requested != expected:
            raise ValueError(f"wrong iteration/requested count on line {line_number}")
        if phase == "BEGIN":
            start = values[0]
            if last_end is not None and start < last_end:
                raise ValueError("iteration timestamps move backwards")
            if first_start is None:
                first_start = start
            phase = "DISPOSED"
        elif phase == "DISPOSED":
            phase = "PASS"
        else:
            end, duration = values
            if end < start or duration != end - start:
                raise ValueError("iteration duration does not match timestamps")
            last_end = end
            iteration += 1
            phase = "BEGIN"
    if result is None:
        raise ValueError("missing complete ordered acceptance run")
    return result


def expected_count(value):
    if not re.fullmatch(r"[0-9]{1,4}", value) or not 1 <= int(value) <= 1000:
        raise argparse.ArgumentTypeError("expected count must be 1 through 1000")
    return int(value)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("path", type=Path)
    parser.add_argument("--expected", type=expected_count, default=100)
    args = parser.parse_args(argv)
    try:
        with args.path.open("rb") as stream:
            raw = stream.read(MAX_BYTES + 1)
        if len(raw) > MAX_BYTES:
            raise ValueError("log exceeds 8 MiB")
        count, ticks, rate = check_log(raw.decode("utf-8"), args.expected)
    except (OSError, UnicodeError, ValueError) as error:
        print(f"session soak: FAIL: {error}", file=sys.stderr)
        return 1
    print(f"session soak: PASS completed={count} total_whole_suite_ticks={ticks} "
          f"ticks_per_second={rate} seconds={ticks / rate:.3f}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
