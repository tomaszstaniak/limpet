"""Exercise the real Makefile with a compiler boundary double, not a source scan."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class NativeBuildConfigTests(unittest.TestCase):
    def test_sdk_or_flags_change_rebuilds_but_identical_config_does_not(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            shutil.copyfile(ROOT / "Makefile", root / "Makefile")
            with (root / "Makefile").open("a") as makefile:
                makefile.write("\nbuild/link-demo: LDFLAGS += -lparent-only\n")
            (root / "tools").mkdir()
            (root / "tools/link_demo.c").write_text("/* compiler input */\n")
            script = ROOT / "tools/write_native_config.py"
            if script.exists():
                shutil.copyfile(script, root / "tools/write_native_config.py")
            (root / "sdk-a").mkdir()
            (root / "sdk-b").mkdir()
            compiler = root / "compiler.py"
            compiler.write_text(
                "import pathlib,sys\n"
                "p=pathlib.Path(sys.argv[sys.argv.index('-o')+1])\n"
                "p.parent.mkdir(parents=True,exist_ok=True)\n"
                "p.write_text(' '.join(sys.argv[1:]))\n"
                "with open('calls','a') as f: f.write('compile\\n')\n"
            )
            command = f"{sys.executable} {compiler}"

            def build(sdk, optimization="-O2", cxx=None, ldflags=None):
                options = [] if ldflags is None else [f"LDFLAGS={ldflags}"]
                result = subprocess.run(
                    [os.environ.get("TEST_MAKE", "make"), "-j2", "build/link-demo",
                     "build/harfbuzz-aros.o", "HB_SRC=tools/link_demo.c", "HB_HEADERS=",
                     f"AROS_GCC={command}", f"AROS_CXX={cxx or command}",
                     f"AROS_SDK={root / sdk}", f"OPTFLAGS={optimization}",
                     f"PYTHON={sys.executable}"] + options,
                    cwd=root, capture_output=True, text=True, timeout=20,
                )
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                return (root / "calls").read_text().splitlines()

            self.assertEqual(len(build("sdk-a")), 2)
            initial = json.loads((root / "build/native-config.json").read_text())
            self.assertNotIn("-lparent-only", initial["ldflags"])
            self.assertEqual(len(build("sdk-a")), 2)
            self.assertEqual(len(build("sdk-b")), 4)
            self.assertIn(str(root / "sdk-b"), (root / "build/link-demo").read_text())
            self.assertIn(str(root / "sdk-b"), (root / "build/harfbuzz-aros.o").read_text())
            self.assertEqual(len(build("sdk-b", "-O0")), 6)
            self.assertEqual(len(build("sdk-a")), 8)
            self.assertEqual(len(build("sdk-a", cxx=command + " --alternate")), 10)
            self.assertEqual(len(build("sdk-a", ldflags="-lchanged")), 12)
            data = json.loads((root / "build/native-config.json").read_text())
            self.assertEqual(data["sdk"], str((root / "sdk-a").resolve()))
            self.assertEqual(data["target"], "mainline-x86_64")
            self.assertIn("-O2", data["cflags"])

    def test_host_only_target_does_not_require_cross_sdk(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            shutil.copyfile(ROOT / "Makefile", root / "Makefile")
            (root / "build").mkdir()
            (root / "tests").mkdir()
            (root / "tests/test_session_test_options.c").write_text("int main(void) { return 0; }\n")
            (root / "tests/session_test_options.h").write_text("/* fixture */\n")
            result = subprocess.run(
                ["make", "build/test_session_test_options", "AROS_GCC=/nonexistent/compiler",
                 "AROS_SDK=/nonexistent/sdk"], cwd=root,
                capture_output=True, text=True, timeout=20,
            )
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertTrue((root / "build/test_session_test_options").is_file())
            self.assertFalse((root / "build/native-config.json").exists())


if __name__ == "__main__":
    unittest.main()
