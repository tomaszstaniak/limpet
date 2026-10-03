"""Refresh a Make prerequisite only when the native build configuration changes.

This records configuration, not proof of SDK ABI or an SDK content manifest.
Do not run different configurations concurrently in the same build directory.
"""
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import sys
import tempfile
import time


def compiler_identity(command):
    words = shlex.split(command)
    executable = shutil.which(words[0]) if words else None
    if not executable:
        raise ValueError(f"Native compiler not found: {command}")
    path = Path(executable).resolve()
    return {"command": command, "executable": str(path),
            "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}


def main():
    destination = Path(sys.argv[1])
    sdk = Path(os.environ["XTERM_NATIVE_SDK"]).resolve(strict=True)
    if not sdk.is_dir():
        raise ValueError(f"SDK is not a directory: {sdk}")
    data = {
        "schema": 1,
        "target": os.environ["XTERM_NATIVE_TARGET"],
        "sdk": str(sdk),
        "cc": compiler_identity(os.environ["XTERM_NATIVE_CC"]),
        "cxx": compiler_identity(os.environ["XTERM_NATIVE_CXX"]),
        "cflags": os.environ["XTERM_NATIVE_CFLAGS"],
        "ldflags": os.environ["XTERM_NATIVE_LDFLAGS"],
        "harfbuzz_flags": os.environ["XTERM_NATIVE_HB_FLAGS"],
        "makefile_sha256": hashlib.sha256(Path("Makefile").read_bytes()).hexdigest(),
        "config_writer_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
    }
    text = json.dumps(data, indent=2, sort_keys=True) + "\n"
    if destination.exists() and destination.read_text() == text:
        return
    # Apple's Make 3.81 compares whole seconds. A changed stamp created in
    # the same second as the preceding executable is otherwise ignored.
    # Wait only on configuration changes, never on an incremental no-op.
    time.sleep(1.01 - time.time() % 1)
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(mode="w", dir=destination.parent,
                                         prefix=".native-config-", delete=False) as output:
            temporary = Path(output.name)
            output.write(text)
        os.replace(temporary, destination)
    finally:
        if temporary is not None and temporary.exists():
            temporary.unlink()


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError) as error:
        sys.exit(f"Native build configuration: {error}")
