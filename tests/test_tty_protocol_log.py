import unittest
from tools.check_tty_protocol_log import BEGIN, END, REQUIRED, validate


class ProtocolLogTests(unittest.TestCase):
    def test_complete_run(self):
        run = "\n".join((BEGIN, *REQUIRED, END)) + "\n"
        self.assertIn("PASS", validate("old failed run\n" + run))

    def test_rejects_missing_native_evidence(self):
        for index in range(len(REQUIRED)):
            with self.subTest(index=index), self.assertRaises(ValueError):
                validate("\n".join((BEGIN, *REQUIRED[:index], *REQUIRED[index+1:], END)))

    def test_rejects_failed_launch_and_truncation(self):
        run = "\n".join((BEGIN, *REQUIRED, END))
        for bad in (run.replace(REQUIRED[0], "controls: file is not executable"),
                    run.replace(REQUIRED[1], "TTY CONTROLS FAIL"),
                    run + "\n" + BEGIN, run + "\nunexpected output",
                    run.replace(END, ""), "x" * (1024 * 1024 + 1)):
            with self.subTest(bad=bad[:80]), self.assertRaises(ValueError):
                validate(bad)
