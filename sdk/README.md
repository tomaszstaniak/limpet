# Limpet SDK 0.6.0

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

Limpet 0.8.0 reports all capabilities exposed by this SDK. The public
geometry packet is supported from Limpet 0.7.0; text capabilities, terminal
identity and the Kitty graphics capability bit require 0.8.0. Earlier
versions still work: an unsupported query or unknown bit is not a promise
that the terminal supports the feature.

Existing source can be rebuilt with SDK 0.6.0. Rebuild objects that use
`LimpetPresentation`: its added text field changes the structure and the
query/decode link names. Programs already built with older SDKs continue
to work with new terminals. See `doc/versioning.md`.

## Build requirements

An ABIv11 x86_64 C toolchain and matching Developer SDK are required.
The prebuilt library uses GCC 10.5 and the 2026.09 SDK. Rebuild it when
using an incompatible toolchain. The release checks are recorded in `doc/sdk/verification-0.6.0.md`.

Project: <https://github.com/tomaszstaniak/limpet>
