import importlib.util
from pathlib import Path
import unittest
import subprocess
import sys
import tempfile

SPEC = importlib.util.spec_from_file_location(
    "check_session_soak", Path(__file__).resolve().parents[1] / "tools/check_session_soak.py")
checker = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(checker)


def sample():
    return """task.resource: open=1 resident=test
SESSION ITERATION 1/2 BEGIN require_break=1 start_ticks=100
ordinary verbose output
SESSION ITERATION 1/2 DISPOSED handles=0 locks=0 pending_read=0 pending_write=0 pending_wait=0 hook=0 hook_target=0 port=0 timer=0 startup_handles=0 references=0
SESSION ITERATION 1/2 PASS end_ticks=150 whole_suite_ticks=50 ticks_per_second=50
SESSION ITERATION 2/2 BEGIN require_break=1 start_ticks=160
SESSION ITERATION 2/2 DISPOSED handles=0 locks=0 pending_read=0 pending_write=0 pending_wait=0 hook=0 hook_target=0 port=0 timer=0 startup_handles=0 references=0
SESSION ITERATION 2/2 PASS end_ticks=240 whole_suite_ticks=80 ticks_per_second=50
SESSION COMPLETE completed=2 requested=2 total_whole_suite_ticks=145 ticks_per_second=50
"""


class SessionSoakLogTests(unittest.TestCase):
    def test_accepts_complete_ordered_strict_run(self):
        result = checker.check_log(sample(), 2)
        self.assertEqual(result, (2, 145, 50))

    def test_rejects_bad_runs(self):
        original = sample()
        mutations = [
            original.replace("require_break=1", "require_break=0", 1),
            original.replace("handles=0", "handles=1", 1),
            original.replace("hook=0", "hook=1", 1),
            original.replace("whole_suite_ticks=50", "whole_suite_ticks=-50", 1),
            original.replace("end_ticks=150", "end_ticks=151", 1),
            original.replace("start_ticks=160", "start_ticks=140", 1),
            original.replace("ticks_per_second=50", "ticks_per_second=0", 1),
            original.replace("ticks_per_second=50", "ticks_per_second=60", 1),
            original.replace("total_whole_suite_ticks=145", "total_whole_suite_ticks=139"),
            original.replace("completed=2", "completed=1"),
            original.replace("requested=2", "requested=3"),
            original.replace("2/2 BEGIN", "1/2 BEGIN"),
            original.replace("1/2 DISPOSED", "2/2 DISPOSED"),
            original.replace(" references=0", "", 1),
            original.replace("SESSION ITERATION 1/2 PASS", "SESSION ITERATION 1/2 UNKNOWN"),
            "\n".join(original.splitlines()[:-1]),
            original + original.splitlines()[-1] + "\n",
            original.replace("SESSION ITERATION 2/2 BEGIN", original.splitlines()[3] + "\nSESSION ITERATION 2/2 BEGIN"),
            original.replace("SESSION COMPLETE", "SESSION COMPLE"),
            original.rstrip("\n"),
            original.replace("start_ticks=100", "start_ticks=-100"),
        ]
        for text in mutations:
            with self.subTest(text=text):
                with self.assertRaises(ValueError):
                    checker.check_log(text, 2)

    def test_rejects_failure_markers_anywhere(self):
        for marker in ("SESSION FAILED", "session deadline:", "session assertion at", "SKIP Ctrl-C"):
            with self.subTest(marker=marker):
                with self.assertRaises(ValueError):
                    checker.check_log(sample() + marker + "\n", 2)

    def test_rejects_wrong_expected_count_and_excess_size(self):
        with self.assertRaises(ValueError):
            checker.check_log(sample(), 100)
        with self.assertRaises(ValueError):
            checker.check_log("x" * (8 * 1024 * 1024 + 1), 2)

    def test_rejects_every_retained_resource(self):
        for field in ("handles", "locks", "pending_read", "pending_write", "pending_wait",
                      "hook", "hook_target", "port", "timer", "startup_handles", "references"):
            with self.subTest(field=field):
                with self.assertRaises(ValueError):
                    checker.check_log(sample().replace(f" {field}=0", f" {field}=1", 1), 2)

    def test_cli_uses_actual_file_and_reports_count_and_duration(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "log"
            path.write_text(sample(), encoding="utf-8")
            command = [sys.executable, str(SPEC.origin), str(path)]
            good = subprocess.run(command + ["--expected", "2"], capture_output=True, text=True)
            self.assertEqual(good.returncode, 0, good.stderr)
            self.assertIn("completed=2", good.stdout)
            self.assertIn("total_whole_suite_ticks=145", good.stdout)
            self.assertIn("seconds=2.900", good.stdout)
            for options in ([], ["--expected", "0"], ["--expected", "1001"],
                            ["--expected", "-1"], ["--expected", "+2"],
                            ["--expected", "abc"]):
                result = subprocess.run(command + options, capture_output=True, text=True)
                self.assertNotEqual(result.returncode, 0)
            path.write_bytes(b"x" * (8 * 1024 * 1024 + 1))
            oversized = subprocess.run(command + ["--expected", "2"], capture_output=True, text=True)
            self.assertNotEqual(oversized.returncode, 0)
            self.assertIn("8 MiB", oversized.stderr)


if __name__ == "__main__":
    unittest.main()
