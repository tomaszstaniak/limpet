# Compatibility and limits

## Systems

| System | Limpet 0.1.0 | SDK 0.1.0 |
| --- | --- | --- |
| AROS ABIv11 x86_64, 2026.09 | released, tested | prebuilt library, examples tested |
| AROS One 1.3 (ABIv11 x86_64) | released, tested at release | not run on this system |
| AROS mainline x86_64 (ABI v1) | not released (an unpublished preview exists) | no library shipped; the sources contain no ABI-specific code but have not been built or run on mainline |
| AROS i386, aarch64, ARM | no build | no build |
| AmigaOS 4, MorphOS, AmigaOS 3 | no build | no build |

ABIv11 and mainline binaries do not run on each other's systems, and a
result on one says nothing about the other. The SDK's sources contain no
ABI-specific code, but a mainline library has to be built with the
mainline toolchain and SDK, and nobody has run it yet.

Tested means: built from the published sources, run on that system under
QEMU, results checked on screen. No test ran on real hardware.

## Terminal versions

| Terminal | Presentation query | Stream features in this documentation |
| --- | --- | --- |
| Limpet 0.1.0 | answers | as listed in the [catalogue](capabilities.md) |
| `CON:` (system console) | `ERROR_ACTION_NOT_KNOWN` | the Amiga console sequences, not the ones documented here |
| other consoles (KingCON and similar) | not tested; expected `ERROR_ACTION_NOT_KNOWN` | unknown |

A program built with SDK 0.1.0 keeps working with later Limpet versions:
the protocol does not change meaning, and the SDK ignores fields it does
not know. A later version may report more; the program will not see it
until it is rebuilt with a newer SDK.

## Standards

Limpet implements **parts** of several specifications. None of the
following is claimed:

- full xterm or VT100/VT220 compatibility;
- full Kitty graphics protocol support;
- POSIX terminal interface or termios on a Limpet 0.1.0 session;
- compatibility with any particular TUI library (ncurses, Ratatui,
  crossterm) on ABIv11.

The exact subsets are in the [protocol reference](protocols.md).

## Limits of Limpet 0.1.0

| Item | Limit |
| --- | --- |
| Text grid | 80 rows, 160 columns at most |
| Scrollback | 256 lines at 160 columns, 512 at 80 columns |
| Images | 16 per terminal, 1024x1024 pixels and 4 MiB each, 16 MiB together |
| Link targets | 2047 bytes, 64 remembered per terminal |
| Kitty control data | 512 bytes per command |

## Known behaviour that affects programs

- **No resize notification.** Ask for the size again.
- **No reply in cooked mode.** Stream queries are answered only in raw
  mode.
- **SGR 30-37 are pens** in a Shell session. Use `38;5;n` for ANSI colours
  (the SDK does).
- **Images do not scroll the screen.** Make room first.
- **Cell size in pixels is not reported.**
- **Ctrl-C** does not reach jobs started with `Run`.
- **`Run` with output redirected to a file** leaves the file empty. This
  is the system's `Run`, not the terminal; it happens in `CON:` too.
- **Programs that open `CON:` themselves** get a `CON:` window, not a
  Limpet one. Limpet does not replace the system console.
