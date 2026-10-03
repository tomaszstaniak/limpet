import hashlib
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1] / "assets/fonts"


class FontAssetTests(unittest.TestCase):
    def test_pinned_fonts_are_unmodified(self):
        expected = {
            "Unifont.otf": "26071c5a97533cefdcbc6b0645e7ee279413049079f09f592b26916ca6c21bf5",
            "NotoEmoji.ttf": "de6c18832938afc99caf132b39d6a30a19bac7f2e812e28db2535b4608d27551",
            "NotoColorEmoji.ttf": "72a635cb3d2f3524c51620cdde406b217204e8a6a06c6a096ff8ed4b5fd6e27b",
        }
        for name, digest in expected.items():
            with self.subTest(font=name):
                self.assertEqual(hashlib.sha256((ROOT / name).read_bytes()).hexdigest(), digest)
