import importlib.util
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("generate_width_table", ROOT / "tools/generate_width_table.py")
generator = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(generator)


class WidthTableTests(unittest.TestCase):
    def test_property_selection_and_union(self):
        text = "# comment\n0041;Na\n3000;F\n4E00..4E02;W # CJK\n00A1;A\n"
        self.assertEqual(generator.ranges(text, {"F", "W"}), [(0x3000, 0x3000), (0x4e00, 0x4e02)])
        self.assertEqual(generator.merge([(5, 7), (1, 3), (3, 4), (10, 10)]), [(1, 7), (10, 10)])

    def test_checked_in_table_matches_pinned_sources(self):
        self.assertEqual(generator.generate(ROOT / "data/unicode-17.0"),
                         (ROOT / "src/unicode_width_table.h").read_text())

    def test_wrong_source_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "EastAsianWidth.txt").write_text("0041;W\n")
            with self.assertRaisesRegex(ValueError, "checksum"):
                generator.generate(root)
