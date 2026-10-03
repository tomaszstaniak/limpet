import unittest
from tools.check_tty_protocol_log import BEGIN, END, REQUIRED
from tools.check_tty_stabilization_log import SERVICE, validate


def fixture():
    lines = ["historical RED", "TTY STABILIZE BEGIN cycles=2"]
    for cycle in (1, 2):
        lines.extend((f"TTY STABILIZE CYCLE {cycle} BEGIN", *SERVICE,
                      BEGIN, *REQUIRED, END, f"TTY STABILIZE CYCLE {cycle} END"))
    lines.append("TTY STABILIZE END cycles=2")
    return lines


class StabilizationLogTests(unittest.TestCase):
    def test_complete(self):
        self.assertEqual(validate("\n".join(fixture()), 2), 2)

    def test_each_missing_marker_is_rejected(self):
        lines = fixture()
        for index in range(1, len(lines)):
            with self.subTest(index=index), self.assertRaises(ValueError):
                validate("\n".join(lines[:index]+lines[index+1:]), 2)

    def test_rejects_wrong_counts_and_failed_launch(self):
        text = "\n".join(fixture())
        for bad in (text.replace("1048568", "1048567", 1),
                    text.replace("TTY ZERO FIFO PASS:", "file is not executable", 1),
                    text+"\nunexpected output", text+"\n"+text):
            with self.subTest(bad=bad[:80]), self.assertRaises(ValueError):
                validate(bad, 2)
        with self.assertRaises(ValueError):
            validate(text, 3)

    def test_rejects_nested_protocol_restart(self):
        text = "\n".join(fixture())
        bad = text.replace(BEGIN, BEGIN+"\nTTY CONTROLS FAIL\n"+BEGIN, 1)
        with self.assertRaises(ValueError):
            validate(bad, 2)
