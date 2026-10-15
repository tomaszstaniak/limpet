# Compatibility and limits

## Systems

| System | Limpet 0.1.0 | Limpet 0.2.0 | Limpet 0.3.0 | SDK 0.2.0 | SDK 0.3.0 |
| --- | --- | --- | --- | --- | --- |
| AROS ABIv11 x86_64, 2026.09 | published, tested | tested | built with its SDK, not run there | prebuilt library, examples tested | built, not run there |
| AROS One 1.3 (ABIv11 x86_64) | published, tested at release | not run | published, tested | not run | `hello` and `title` tested |
| AROS mainline x86_64 (ABI v1) | not released (an unpublished preview exists) | not released | not built | no library shipped; the sources contain no ABI-specific code but have not been built or run on mainline | as SDK 0.2.0 |
| AROS i386, aarch64, ARM | no build | no build | no build | no build | no build |
| AmigaOS 4, MorphOS, AmigaOS 3 | no build | no build | no build | no build | no build |

ABIv11 and mainline binaries do not run on each other's systems, and a
result on one says nothing about the other. The SDK's sources contain no
ABI-specific code, but a mainline library has to be built with the
mainline toolchain and SDK, and nobody has run it yet.

Tested means: built from the published sources, run on that system under
QEMU, results checked on screen. No test ran on real hardware.

## Terminal versions

| Terminal | Presentation query | Stream features in this documentation |
| --- | --- | --- |
| Limpet 0.3.0 | answers, with input modes and features | as listed in the [catalogue](capabilities.md) |
| Limpet 0.2.0, 0.2.1 | answers, with input modes | as listed, except entries marked "Since 0.3.0" |
| Limpet 0.1.0 | answers, without input modes | as listed, except entries marked "Since 0.2.0" |
| `CON:` (system console) | `ERROR_ACTION_NOT_KNOWN` | the Amiga console sequences, not the ones documented here |
| other consoles (KingCON and similar) | not tested; expected `ERROR_ACTION_NOT_KNOWN` | unknown |

Which client works with which terminal, and what a rebuild is needed for:
[versions and stability](versioning.md).

## Standards

Limpet implements **parts** of several specifications. None of the
following is claimed:

- full xterm or VT100/VT220 compatibility;
- full Kitty graphics protocol support;
- POSIX terminal interface or termios on a Limpet Shell session;
- compatibility with any particular TUI library (ncurses, Ratatui,
  crossterm) on ABIv11.

The exact subsets are in the [protocol reference](protocols.md).

## Limits of Limpet 0.1.0 to 0.4.1

| Item | Limit |
| --- | --- |
| Text grid | 80 rows, 160 columns at most |
| Scrollback | 256 lines at 160 columns, 512 at 80 columns; from the 0.5.0 candidate set by `scrollback` (100 to 1000 lines at 160 columns, [settings](search-and-settings.md#settings)) |
| Command history | 32 commands; from the 0.5.0 candidate set by `history` (1 to 500) |
| Images | 16 per terminal, 1024x1024 pixels and 4 MiB each, 16 MiB together |
| Link targets | 2047 bytes, 64 remembered per terminal |
| Kitty control data | 512 bytes per command |

## Known behaviour that affects programs

- **No resize signal.** Ask for the size again, or (0.2.0) turn on resize
  reports in raw mode.
- **Input modes end with raw mode** (0.2.0). Set 2004, 2048, 2100 and mouse
  tracking again after each switch to raw mode.
- **No reply in cooked mode.** Stream queries are answered only in raw
  mode.
- **SGR 30-37 are pens** in a Shell session. Use `38;5;n` for ANSI colours
  (the SDK does).
- **Images and the bottom edge.** Limpet 0.3.0 scrolls to show a `C=0`
  image whole; 0.1.0 to 0.2.1 do not. Make room first to work with all.
- **Cell size in pixels is not reported.**
- **Ctrl-C** does not reach jobs started with `Run`.
- **Window title** (0.3.0): only after the presentation reply confirmed
  `XTY_FEATURE_TITLE`; a title lasts while the command that sent it runs.
- **`Run` with output redirected to a file** leaves the file empty. This
  is the system's `Run`, not the terminal; it happens in `CON:` too.
- **Programs that open `CON:` themselves** get a `CON:` window, not a
  Limpet one. Limpet does not replace the system console.
