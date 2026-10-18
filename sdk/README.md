# Limpet SDK 0.3.0

The SDK provides a presentation query and helpers for colour, progress
lines, links, images and window titles in the Limpet terminal. Programs
using only plain-text I/O do not need it.

Query the output handle to select supported features and use plain text
when the query is unsupported or output is redirected. ANSI palette colours
are encoded correctly when the console uses Workbench pens.

Target: **AROS ABIv11, x86_64**. Licence: MIT (`LICENSE.txt`).

## Contents

| Path | |
| --- | --- |
| `include/limpet/limpet.h` | the API |
| `include/limpet/tty_control.h` | the wire protocol, the same file the terminal is built from |
| `lib/abiv11/libLimpet.a` | prebuilt library |
| `src/` | library source: two C files, needing only `dos.library` and the C library |
| `examples/` | `hello`, `colors`, `progress`, `geometry`, `hyperlink`, `image`, `keys`, `title` |
| `tests/host/` | stand-in AROS headers for compiling the library on a desktop host |
| `doc/` | documentation |
| `MANIFEST.sha256` | checksums of all files |

## Start here

1. `doc/sdk/getting-started.md`: build the examples and run `hello`.
2. `doc/sdk/choosing.md`: which interface fits your program.
3. `doc/capabilities.md`: features, detection and limits.
4. `doc/sdk/api.md`: the reference.

Quick build:

    make CC=/path/to/x86_64-aros-gcc AROS_SDK=/path/to/SDK/Development \
         ABI=abiv11 examples

Copy `build/abiv11/hello` to AROS and run it from the Shell in a Limpet
window and from the Shell in a `CON:` window.

## Compatibility

Works against Limpet 0.3.0 (every feature), 0.2.0 and 0.2.1 (everything
except the window title, which they do not confirm) and 0.1.0 (also
without the input modes, which it reports as unknown). Code written for an
earlier SDK compiles unchanged, but objects must be rebuilt with this
version: `LimpetPresentation` grew again. Details: `doc/versioning.md`.

## Build requirements

An ABIv11 x86_64 C toolchain and matching Developer SDK are required.
The prebuilt library uses GCC 10.5 and the 2026.09 SDK. Rebuild it when
using an incompatible toolchain. Verification records are in
`doc/sdk/verification-0.3.0.md` and `doc/sdk/verification-0.2.0.md`.

Project: <https://github.com/tomaszstaniak/limpet>
