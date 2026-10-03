#!/usr/bin/env python3
"""Validate format-2 snapshots and generate portable C replay test vectors."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

REQUIRED_FIXTURES = ("plain-text", "split-escape", "utf8", "amiga-csi")


def require(condition, message):
    if not condition:
        raise ValueError(message)


def integer(value, low, high, label):
    require(type(value) is int and low <= value <= high, f"invalid {label}")
    return value


def object_fields(value, allowed, label):
    require(isinstance(value, dict), f"{label} must be an object")
    require(not (value.keys() - allowed), f"unknown field in {label}")


def local_path(root, name):
    require(isinstance(name, str) and bool(name), "expected a relative file path")
    path = (root / name).resolve()
    require(path.is_relative_to(root.resolve()), "path must remain inside fixture directory")
    return path


def read_json(path):
    return json.loads(path.read_text(encoding="utf-8"))


def dimensions(value):
    require(isinstance(value, list) and len(value) == 2, "size must be [rows, columns]")
    return (integer(value[0], 1, 80, "rows"), integer(value[1], 1, 160, "columns"))


def style(value):
    object_fields(value, {"fg", "bg", "bold"}, "style")
    return [integer(value.get("fg", 7), 0, 255, "foreground"),
            integer(value.get("bg", 0), 0, 255, "background"),
            integer(value.get("bold", 0), 0, 1, "bold")]


def snapshot(path, rows, columns, ready):
    data = read_json(path)
    object_fields(data, {"lines", "widths", "cursor", "style", "cells", "history_count",
                         "view_offset", "wrap_pending"}, "snapshot")
    lines = data.get("lines")
    require(isinstance(lines, list) and len(lines) == rows, "snapshot row count mismatch")
    require(all(isinstance(line, str) and len(line) == columns for line in lines),
            "snapshot column count mismatch")
    require(all(ord(ch) >= 32 and ord(ch) != 127 for line in lines for ch in line),
            "snapshot contains control characters")
    require(all(not 0xD800 <= ord(ch) <= 0xDFFF for line in lines for ch in line),
            "snapshot contains a surrogate, not a Unicode scalar")
    cursor = data.get("cursor")
    require(isinstance(cursor, list) and len(cursor) == 2, "cursor must be [row, column]")
    cursor = [integer(cursor[0], 0, rows - 1, "cursor row"),
              integer(cursor[1], 0, columns - 1, "cursor column")]
    widths = data.get("widths", ["1" * columns for _ in range(rows)])
    require(isinstance(widths, list) and len(widths) == rows and all(
        isinstance(line, str) and len(line) == columns and set(line) <= {"0", "1", "2"}
        for line in widths), "invalid cell widths")
    cells = [[ord(ch), 7, 0, 0, int(width), [] if ch == ' ' else [ord(ch)]]
             for line, spans in zip(lines, widths) for ch, width in zip(line, spans)]
    overrides = data.get("cells", [])
    require(isinstance(overrides, list), "cells must be a list")
    seen = set()
    for cell in overrides:
        object_fields(cell, {"row", "column", "fg", "bg", "bold", "cluster"}, "cell override")
        row = integer(cell.get("row"), 0, rows - 1, "cell row")
        col = integer(cell.get("column"), 0, columns - 1, "cell column")
        require((row, col) not in seen, "duplicate cell override")
        seen.add((row, col))
        cells[row * columns + col][1:4] = style({k: cell[k] for k in ("fg", "bg", "bold") if k in cell})
        if "cluster" in cell:
            cluster = cell["cluster"]
            require(isinstance(cluster, str) and 1 <= len(cluster) <= 16,
                    "cluster must contain 1..16 scalars")
            require(all(ord(ch) >= 32 and ord(ch) != 127 and not 0xD800 <= ord(ch) <= 0xDFFF
                        for ch in cluster), "invalid cluster scalar")
            target = cells[row * columns + col]
            require(target[4] != 0 and ord(cluster[0]) == target[0],
                    "cluster must match its lead cell")
            target[5] = list(map(ord, cluster))
    for row in range(rows):
        for col in range(columns):
            index = row * columns + col
            cell = cells[index]
            if cell[4] == 0:
                require(col > 0 and cells[index - 1][4] == 2 and cell[0] == 32,
                        "orphan or nonblank continuation")
                require(cell[1:4] == cells[index - 1][1:4], "wide pair style mismatch")
            elif cell[4] == 2:
                require(col + 1 < columns and cells[index + 1][4] == 0,
                        "wide lead requires a continuation in the same row")
    history = integer(data.get("history_count", 0), 0, 256, "history count")
    view = integer(data.get("view_offset", 0), 0, history, "view offset")
    wrap = integer(data.get("wrap_pending", 0), 0, 1, "wrap pending")
    return {"rows": rows, "columns": columns, "cells": cells, "cursor": cursor,
            "style": style(data.get("style", {})), "history_count": history,
            "view_offset": view, "wrap_pending": wrap}


def load_manifest(path):
    document = read_json(path)
    object_fields(document, {"format", "fixtures"}, "manifest")
    require(document.get("format") == 2, "manifest format must be 2")
    fixtures = document.get("fixtures")
    require(isinstance(fixtures, list) and len(fixtures) <= 64, "fixtures must be a list of at most 64 entries")
    result, seen = [], set()
    for item in fixtures:
        object_fields(item, {"id", "status", "kind", "reason", "size", "history_lines",
                             "input", "input_hex", "sha256", "expected", "actions"}, "fixture")
        name = item.get("id")
        require(isinstance(name, str) and re.fullmatch(r"[a-z0-9-]+", name), "invalid fixture id")
        require(name not in seen, f"duplicate fixture: {name}")
        seen.add(name)
        require(("input" in item) != ("input_hex" in item), f"{name}: requires exactly one input source")
        if "input_hex" in item:
            try:
                raw = bytes.fromhex(item["input_hex"])
            except (ValueError, TypeError):
                raise ValueError(f"invalid input_hex for {name}") from None
        else:
            raw = local_path(path.parent, item["input"]).read_bytes()
        require(len(raw) <= 8192, f"{name}: input exceeds 8192 bytes")
        require(hashlib.sha256(raw).hexdigest() == item.get("sha256"), f"sha256 mismatch for {name}")
        ready = item.get("status") == "ready"
        require(item.get("status") in ("ready", "deferred"), f"{name}: invalid status")
        require(item.get("kind") in ("input", "output"), f"{name}: invalid kind")
        if not ready:
            require(isinstance(item.get("reason"), str) and bool(item["reason"].strip()),
                    f"{name}: deferred fixture requires reason")
        require(not (item["kind"] == "input" and ready), "input fixtures must be deferred")
        expected_path = local_path(path.parent, item.get("expected"))
        if item["kind"] == "input":
            expected = read_json(expected_path)
            object_fields(expected, {"events"}, "input expectation")
            require(isinstance(expected.get("events"), list) and all(
                isinstance(e, str) for e in expected["events"]), "events must be a list of strings")
            require(not item.get("actions"), "input fixture cannot have screen actions")
            result.append({"id": name, "ready": False, "reason": item["reason"]})
            continue
        rows, columns = dimensions(item.get("size"))
        history_lines = integer(item.get("history_lines", 0), 0, 256, "history lines")
        first = snapshot(expected_path, rows, columns, ready)
        actions = item.get("actions", [])
        require(isinstance(actions, list) and len(actions) <= 32, "too many actions")
        normalized = []
        for action in actions:
            object_fields(action, {"resize", "scroll", "expected"}, "action")
            require(("resize" in action) != ("scroll" in action), "action needs one resize or scroll")
            if "resize" in action:
                rows, columns = dimensions(action["resize"])
                operation, argument = 1, 0
            else:
                operation = 2
                argument = integer(action["scroll"], -256, 256, "scroll")
            normalized.append({"operation": operation, "argument": argument,
                               "snapshot": snapshot(local_path(path.parent, action.get("expected")),
                                                    rows, columns, ready)})
        result.append({"id": name, "ready": ready, "reason": item.get("reason", ""),
                       "raw": raw, "initial": first, "history_lines": history_lines,
                       "actions": normalized})
    for name in REQUIRED_FIXTURES:
        require(name in seen, f"missing required fixture: {name}")
    require(any(f["ready"] for f in result), "at least one ready fixture is required")
    return result


def generate_header(fixtures):
    lines = ['/* Generated from fixture JSON. Do not edit. */', '#include "replay_case.h"']
    cases, deferred = [], []
    for index, fixture in enumerate(fixtures):
        if not fixture["ready"]:
            deferred.append(json.dumps(fixture["id"] + ": " + fixture["reason"]))
            continue
        prefix = f"fixture_{index}"
        raw = fixture["raw"]
        lines.append(f"static const unsigned char {prefix}_input[] = {{" +
                     ", ".join(str(b) for b in raw or b"\0") + "};")
        def emit_snapshot(snap, suffix):
            symbol = prefix + suffix
            def emit_cell(cell):
                return ("{" + ",".join(map(str, cell[:5])) + ",{" +
                        ",".join(map(str, cell[5][1:] or [0])) + "}," + str(len(cell[5])) + ",0,0,0,0,0}")
            lines.append(f"static const XTermCell {symbol}_cells[] = {{" +
                         ", ".join(emit_cell(cell) for cell in snap["cells"]) + "};")
            values = [snap["rows"], snap["columns"], *snap["cursor"], snap["history_count"],
                      snap["view_offset"], snap["wrap_pending"]]
            return ("{" + ",".join(map(str, values)) + ",{32," +
                    ",".join(map(str, snap["style"])) + ",1,{0},0,0,0,0,0,0}," + symbol + "_cells}")
        initial = emit_snapshot(fixture["initial"], "_initial")
        actions = []
        for step, action in enumerate(fixture["actions"]):
            snap = emit_snapshot(action["snapshot"], f"_step{step}")
            actions.append("{" + f'{action["operation"]},{action["argument"]},' + snap + "}")
        if actions:
            lines.append(f"static const ReplayAction {prefix}_actions[] = {{" + ",".join(actions) + "};")
        cases.append("{" + json.dumps(fixture["id"]) + f",{prefix}_input,{len(raw)}," +
                     str(fixture["history_lines"]) + "," + initial + "," +
                     (prefix + "_actions" if actions else "NULL") + f",{len(actions)}" + "}")
    lines.append("static const ReplayCase replay_cases[] = {" + ",".join(cases) + "};")
    lines.append("static const char *const deferred_cases[] = {" + ",".join(deferred or ["NULL"]) + "};")
    lines.append(f"#define DEFERRED_COUNT {len(deferred)}u")
    return "\n".join(lines) + "\n"


def main(arguments=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifest", type=Path)
    parser.add_argument("--generate", type=Path, help="write portable C vectors")
    args = parser.parse_args(arguments)
    try:
        fixtures = load_manifest(args.manifest)
        if args.generate:
            content = generate_header(fixtures)
            if not args.generate.exists() or args.generate.read_text(encoding="ascii") != content:
                args.generate.write_text(content, encoding="ascii")
        ready = sum(f["ready"] for f in fixtures)
        print(f"validated {len(fixtures)} fixture(s): {ready} ready, {len(fixtures) - ready} deferred")
        return 0
    except (ValueError, OSError, UnicodeError) as error:
        print(f"fixture error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
