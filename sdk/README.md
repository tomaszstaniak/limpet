# Limpet SDK 0.3.0

Limpet is a terminal window for the AROS Shell. The Shell and its commands
stay the same; the window adds UTF-8 text, scrollback that rewraps on
resize, copy and paste through the system clipboard, clickable links,
256 and 24-bit colour, inline images, a window title that names the
running program and its state and, for full-screen programs, bracketed
paste, resize and wheel reports. See `doc/why-limpet.md`.

The SDK checks whether the output goes to Limpet: there a program uses
these features, and in `CON:` or a file it prints plain text.
Target: **AROS ABIv11, x86_64**. Licence: MIT (`LICENSE.txt`).

## Do you need it?

- **No**, if your program prints text and reads lines. It already works in
  Limpet, Unicode included.
- **Yes**, if you want colour, a progress line, links, images, a window
  title or full-screen input modes, and want your program to stay correct in `CON:` and when
  its output goes to a file.

## What is specific to Limpet on AROS

- Detection is one DOS packet on your own `Output()`. It never reads the
  keyboard, and `CON:` refuses it at once, so there are no timeouts and no
  stray characters in the Shell.
- SGR 30-37 select Workbench pens, as in `CON:`, so the Shell prompt keeps
  its colours. The SDK sends ANSI colours in a form that keeps their
  meaning.
- Input modes for full-screen programs end when the program returns the
  console to cooked mode, so the Shell never inherits them.

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
3. `doc/capabilities.md`: every feature, how to detect it, and how far it
   was tested.
4. `doc/sdk/api.md`: the reference.

Quick build:

    make CC=/path/to/x86_64-aros-gcc AROS_SDK=/path/to/SDK/Development \
         ABI=abiv11 examples

Copy `build/abiv11/hello` to AROS and run it from the Shell in a Limpet
window and from the Shell in a `CON:` window.

## Compatibility in one paragraph

Works against Limpet 0.3.0 (every feature), 0.2.0 and 0.2.1 (everything
except the window title, which they do not confirm) and 0.1.0 (also
without the input modes, which it reports as unknown). Code written for an
earlier SDK compiles unchanged, but objects must be rebuilt with this
version: `LimpetPresentation` grew again. Details: `doc/versioning.md`.

## Tested

Built with GCC 10.5 and the ABIv11 2026.09 Developer SDK. SDK 0.3.0:
`hello`, `title`, `hyperlink` and `image` ran on AROS One 1.3 (ABIv11)
x86_64 under QEMU in Limpet 0.3.0, 2026-10-11;
host tests check the reply decoding against SDK 0.1 and 0.2 clients. The
other examples are unchanged since SDK 0.2.0, whose examples all ran on
AROS ABIv11 2026.09 in Limpet 0.2.0 and in `CON:` (2026-10-07). Not tested
on mainline AROS or on real hardware. Details: `doc/verification-0.3.0.md`
and `doc/sdk/verification-0.2.0.md`.

Project: <https://github.com/tomaszstaniak/limpet>
