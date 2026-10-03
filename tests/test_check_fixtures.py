import copy
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import shlex
import tempfile
import unittest

PROJECT = Path(__file__).resolve().parents[1]
CHECKER = PROJECT / "tools/check_fixtures.py"
SPEC = importlib.util.spec_from_file_location("check_fixtures", CHECKER)
checker = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(checker)


class FixtureManifestTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.document = {"format": 2, "fixtures": []}
        for name in ("plain-text", "split-escape", "utf8", "amiga-csi"):
            data = b"A"
            self.document["fixtures"].append({
                "id": name, "status": "ready", "kind": "output",
                "size": [2, 4], "input_hex": data.hex(),
                "sha256": hashlib.sha256(data).hexdigest(),
                "expected": name + ".expected",
            })
            self.write_expected(name, {"lines": ["A   ", "    "], "cursor": [0, 1]})

    def write_expected(self, name, expected):
        (self.root / (name + ".expected")).write_text(json.dumps(expected), encoding="utf-8")

    def run_check(self, document=None, generate=False):
        path = self.root / "manifest.json"
        path.write_text(json.dumps(self.document if document is None else document), encoding="utf-8")
        args = [sys.executable, str(CHECKER), str(path)]
        if generate:
            args += ["--generate", str(self.root / "fixtures.h")]
        return subprocess.run(args, capture_output=True, text=True)

    def test_valid_snapshots_generate_compilable_vectors(self):
        result = self.run_check(generate=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("4 ready, 0 deferred", result.stdout)
        self.assertIn("replay_cases", (self.root / "fixtures.h").read_text())

    def test_rejects_broken_wide_pairs(self):
        invalid = [
            {"widths": ["0111", "1111"]},
            {"widths": ["1112", "1111"]},
            {"widths": ["2111", "1111"]},
            {"widths": ["2011", "1111"], "lines": ["AX  ", "    "]},
            {"widths": ["2011", "1111"], "cells": [{"row": 0, "column": 1, "fg": 1}]},
            {"widths": ["3011", "1111"]},
            {"widths": ["111", "1111"]},
        ]
        for changes in invalid:
            with self.subTest(changes=changes):
                snap = {"lines": ["A   ", "    "], "cursor": [0, 1]}
                snap.update(changes)
                self.write_expected("plain-text", snap)
                self.assertNotEqual(self.run_check().returncode, 0)

    def test_cluster_overrides_are_bounded_and_preserve_scalars(self):
        snap = {"lines": ["e   ", "    "], "cursor": [0, 1],
                "cells": [{"row": 0, "column": 0, "cluster": "é"}]}
        self.write_expected("plain-text", snap)
        result = self.run_check(generate=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        for cluster in ("", "x́", "e" + "́" * 16, "e\ud800", "e\n"):
            snap["cells"][0]["cluster"] = cluster
            self.write_expected("plain-text", snap)
            self.assertNotEqual(self.run_check().returncode, 0)

    def test_unicode_scalars_are_preserved_and_surrogates_rejected(self):
        item = self.document["fixtures"][2]
        raw = "ą€😀".encode("utf-8")
        item["input_hex"] = raw.hex()
        item["sha256"] = hashlib.sha256(raw).hexdigest()
        self.write_expected("utf8", {"lines": ["ą€😀 ", "    "], "cursor": [0, 3]})
        result = self.run_check(generate=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        for invalid in ("\ud800", "\udfff"):
            self.write_expected("utf8", {"lines": [invalid + "   ", "    "], "cursor": [0, 1]})
            self.assertNotEqual(self.run_check().returncode, 0)

    def test_real_c_runner_rejects_wrong_glyph_cursor_and_colour(self):
        cases = [
            ({"lines": ["A   ", "    "], "cursor": [0, 1]}, 0),
            ({"lines": ["Z   ", "    "], "cursor": [0, 1]}, 1),
            ({"lines": ["A   ", "    "], "cursor": [0, 2]}, 1),
            ({"lines": ["A   ", "    "], "cursor": [0, 1],
              "cells": [{"row": 0, "column": 0, "fg": 1}]}, 1),
            # Structurally valid, but wrong width for ASCII: runner must fail.
            ({"lines": ["A   ", "    "], "widths": ["2011", "1111"], "cursor": [0, 1]}, 1),
            ({"lines": ["A   ", "    "], "cursor": [0, 1],
              "cells": [{"row": 0, "column": 0, "cluster": "Á"}]}, 1),
        ]
        for expected, exit_code in cases:
            with self.subTest(expected=expected):
                self.write_expected("plain-text", expected)
                manifest = self.root / "manifest.json"
                manifest.write_text(json.dumps(self.document), encoding="utf-8")
                header = checker.generate_header(checker.load_manifest(manifest))
                (self.root / "replay_fixtures.h").write_text(header, encoding="ascii")
                binary = self.root / "runner"
                command = shlex.split(os.environ.get("HOST_CC", "cc")) + [
                    "-std=c99", "-Wall", "-Wextra", "-Werror",
                    "-I" + str(self.root), "-I" + str(PROJECT / "tests"),
                    "-DUTF8PROC_STATIC", "-I" + str(PROJECT / "vendor/utf8proc"),
                    str(PROJECT / "tests/test_replay.c"),
                    str(PROJECT / "vendor/utf8proc/utf8proc.c"),
                    str(PROJECT / "src/screen_model.c"),
                    str(PROJECT / "src/hyperlink.c"), "-o", str(binary)]
                built = subprocess.run(command, capture_output=True, text=True)
                self.assertEqual(built.returncode, 0, built.stderr)
                result = subprocess.run([str(binary)], capture_output=True, text=True)
                self.assertEqual(result.returncode, exit_code, result.stdout + result.stderr)
                if exit_code:
                    self.assertIn("FAIL plain-text", result.stderr)

    def test_bad_manifest_shapes_are_reported_without_traceback(self):
        for document in (None, [], {"format": 2, "fixtures": {}}, {"format": 99, "fixtures": []}):
            with self.subTest(document=document):
                result = self.run_check(document if document is not None else [])
                self.assertNotEqual(result.returncode, 0)
                self.assertNotIn("Traceback", result.stderr)

    def test_reports_missing_required_fixture(self):
        self.document["fixtures"] = []
        self.assertIn("missing required fixture: plain-text", self.run_check().stderr)

    def test_rejects_duplicate_ids(self):
        self.document["fixtures"].append(copy.deepcopy(self.document["fixtures"][0]))
        self.assertIn("duplicate fixture", self.run_check().stderr)

    def test_rejects_wrong_digest(self):
        self.document["fixtures"][0]["sha256"] = "0" * 64
        self.assertIn("sha256 mismatch", self.run_check().stderr)

    def test_rejects_invalid_hex(self):
        self.document["fixtures"][0]["input_hex"] = "xyz"
        self.assertIn("invalid input_hex", self.run_check().stderr)

    def test_rejects_ambiguous_input(self):
        self.document["fixtures"][0]["input"] = "another-file"
        self.assertIn("exactly one", self.run_check().stderr)

    def test_rejects_escaping_expectation_path(self):
        self.document["fixtures"][0]["expected"] = "../outside"
        self.assertIn("inside fixture directory", self.run_check().stderr)

    def test_rejects_wrong_width_and_out_of_range_cursor(self):
        for expected in (
            {"lines": ["short", "    "], "cursor": [0, 1]},
            {"lines": ["A   ", "    "], "cursor": [2, 0]},
            {"lines": ["A   ", "    "], "cursor": [0, True]},
            {"lines": ["A   ", "    "], "cursor": [0, 1], "cells": [{"row": 0, "column": 9}]},
        ):
            self.write_expected("plain-text", expected)
            self.assertNotEqual(self.run_check().returncode, 0)

    def test_deferred_cases_require_reason_and_are_not_reported_as_passes(self):
        fixture = self.document["fixtures"][2]
        fixture["status"] = "deferred"
        self.assertIn("reason", self.run_check().stderr)
        fixture["reason"] = "Unicode renderer is not implemented"
        result = self.run_check(generate=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("3 ready, 1 deferred", result.stdout)

    def test_input_events_cannot_be_executed_as_terminal_output(self):
        self.document["fixtures"][3]["kind"] = "input"
        self.assertIn("input fixtures must be deferred", self.run_check().stderr)

    def test_rejects_invalid_action_dimensions(self):
        self.document["fixtures"][0]["actions"] = [{"resize": [0, 8], "expected": "plain-text.expected"}]
        self.assertNotEqual(self.run_check().returncode, 0)

    def test_rejects_invalid_colour_and_unknown_snapshot_field(self):
        for expected in (
            {"lines": ["A   ", "    "], "cursor": [0, 1], "style": {"fg": 256}},
            {"lines": ["A   ", "    "], "cursor": [0, 1], "style": {"bg": -1}},
            {"lines": ["A   ", "    "], "cursor": [0, 1], "curser": [0, 1]},
        ):
            self.write_expected("plain-text", expected)
            self.assertNotEqual(self.run_check().returncode, 0)

    def test_accepts_indexed_colour_snapshots(self):
        self.write_expected("plain-text", {
            "lines": ["A   ", "    "], "cursor": [0, 1],
            "style": {"fg": 255, "bg": 16},
            "cells": [{"row": 0, "column": 0, "fg": 196, "bg": 243}],
        })
        result = self.run_check(generate=True)
        self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    unittest.main()
