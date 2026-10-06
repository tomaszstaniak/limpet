# Limpet SDK 0.1.0

For AROS programs that want to know what the console renders before they
add colour, progress lines, links or images. In Limpet they get an answer;
in `CON:` and in files they get a clean "no", and print plain text.

Target: **AROS ABIv11, x86_64**. Licence: MIT (`LICENSE.txt`).

## Contents

| Path | |
| --- | --- |
| `include/limpet/limpet.h` | the API |
| `include/limpet/tty_control.h` | the wire protocol, the same file the terminal is built from |
| `lib/abiv11/libLimpet.a` | prebuilt library |
| `src/` | library source: two C files, needing only `dos.library` and the C library |
| `examples/` | `hello`, `colors`, `progress`, `geometry`, `hyperlink`, `image`, `keys` |
| `tests/host/` | stand-in AROS headers for compiling the library on a desktop host |
| `doc/` | documentation; start with `doc/sdk/getting-started.md` |
| `MANIFEST.sha256` | checksums of all files |

## Quick start

    make CC=/path/to/x86_64-aros-gcc AROS_SDK=/path/to/SDK/Development \
         ABI=abiv11 examples

Copy `build/abiv11/hello` to AROS and run it from the Shell in a Limpet
window and from the Shell in a `CON:` window. Then read `doc/sdk/choosing.md`.

## Tested

Built with GCC 10.5 and the ABIv11 2026.09 Developer SDK. All examples run
on AROS ABIv11 2026.09 x86_64 under QEMU, in Limpet 0.1.0 and in `CON:`,
on 2026-10-05. Not tested on mainline AROS, on AROS One 1.3, or on real
hardware. Details: `doc/sdk/verification-0.1.0.md`.

Project: <https://github.com/tomaszstaniak/limpet>
