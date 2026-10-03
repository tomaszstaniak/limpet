"""Validate bounded flood telemetry; never infer success from surviving rows."""
import argparse
import csv
import json

STAGES = "due offer dispatch admit peer parsed uploaded readback".split()


def validate_schedule(rows, seconds, hz):
    if not rows or not 1 <= seconds <= 600 or hz <= 0:
        raise ValueError("invalid schedule configuration")
    epoch = rows[0]["due"]
    expected = [epoch + i * hz // 20 for i in range(seconds * 20)]
    expected.append(epoch + seconds * hz)
    for minute in range(60, seconds, 60):
        expected += [epoch + minute * hz] * 1025
    if sorted(row["due"] for row in rows) != sorted(expected):
        raise ValueError("missing/shifted deadline or missing burst")


def validate(rows, expected):
    if not 0 < expected <= 65536 or len(rows) != expected:
        raise ValueError("missing/excess samples or invalid capacity")
    pairs = [("readback", "offer"), ("readback", "due"), ("uploaded", "offer")]
    pairs += list(zip(STAGES[1:], STAGES[:-1]))
    result = {f"{end}-{start}": [] for end, start in pairs}
    for seq, row in enumerate(rows, 1):
        if row["sequence"] != seq:
            raise ValueError(f"sample {seq}: sequence mismatch")
        ticks = [row[field] for field in STAGES]
        if any(t <= 0 or t > 2**64 - 1 for t in ticks) or ticks != sorted(ticks):
            raise ValueError(f"sample {seq}: missing/nonmonotonic timestamp")
        for end, start in pairs:
            result[f"{end}-{start}"].append(row[end] - row[start])
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv")
    parser.add_argument("--expected", type=int, required=True)
    parser.add_argument("--hz", type=int, required=True)
    parser.add_argument("--seconds", type=int)
    args = parser.parse_args()
    if args.hz <= 0:
        parser.error("frequency must be positive")
    with open(args.csv, newline="", encoding="ascii") as stream:
        rows = []
        for row in csv.DictReader(stream):
            if len(rows) == 65536:
                raise ValueError("sample capacity exceeded")
            rows.append({key: int(value) for key, value in row.items()})
    stages = validate(rows, args.expected)
    if args.seconds is not None:
        validate_schedule(rows, args.seconds, args.hz)
    report = {}
    for stage, ticks in stages.items():
        ordered = sorted(ticks)
        report[stage] = {f"p{p}_ms": ordered[(len(ticks)*p+99)//100-1]*1000/args.hz
                         for p in (50, 95, 99, 100)}
        report[stage]["max_sequence"] = ticks.index(max(ticks)) + 1
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
