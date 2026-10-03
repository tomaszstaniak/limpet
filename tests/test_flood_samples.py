import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location(
    "flood_samples", Path(__file__).parents[1] / "tools/check_flood_samples.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class FloodSamplesTests(unittest.TestCase):
    def rows(self):
        fields = "sequence due offer dispatch admit peer parsed uploaded readback".split()
        return [dict(zip(fields, [i, 10, 11, 12, 13, 14, 15, 16, 17]))
                for i in range(1, 4)]

    def test_complete(self):
        result = module.validate(self.rows(), 3)
        self.assertEqual(result["readback-offer"], [6, 6, 6])
        self.assertEqual(result["dispatch-offer"], [1, 1, 1])

    def test_missing_duplicate_and_reordered(self):
        rows = self.rows()
        for bad in [rows[:-1], rows + rows[:1], [rows[1], rows[0], rows[2]]]:
            with self.subTest(rows=bad), self.assertRaises(ValueError):
                module.validate(bad, 3)

    def test_incomplete_or_nonmonotonic_stage(self):
        for field in self.rows()[0]:
            rows = self.rows()
            rows[1][field] = 0
            with self.subTest(field=field), self.assertRaises(ValueError):
                module.validate(rows, 3)
        rows = self.rows()
        rows[1]["admit"] = 18
        with self.assertRaises(ValueError):
            module.validate(rows, 3)

    def test_capacity(self):
        for size in [0, 65537]:
            with self.assertRaises(ValueError):
                module.validate([], size)

    def test_schedule_rejects_skipped_deadlines(self):
        rows = [{"due": 100 + i * 5} for i in range(21)]
        module.validate_schedule(rows, 1, 100)
        rows[10]["due"] += 1
        with self.assertRaises(ValueError):
            module.validate_schedule(rows, 1, 100)

    def test_schedule_requires_minute_burst(self):
        rows = [{"due": 100 + i * 5} for i in range(1221)]
        with self.assertRaises(ValueError):
            module.validate_schedule(rows, 61, 100)
        rows += [{"due": 6100}] * 1025
        module.validate_schedule(rows, 61, 100)


if __name__ == "__main__":
    unittest.main()
