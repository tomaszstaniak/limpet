# Compatibility and limits

## Systems

The current terminal release, **0.7.0**, has three builds:

| Build | System | Checked on |
| --- | --- | --- |
| `abiv11` | AROS ABIv11 x86_64 (AROS One and similar) | AROS One 1.3 |
| `v1-x86_64` | mainline AROS (ABIv1) x86_64 | mainline built from `74a2d80b` (2026-09-04), under QEMU |
| `v1-aarch64` (test build) | AROS for the Raspberry Pi (AArch64, ABIv1) | the 2026-08-22 Pi system under QEMU's Pi 3 model; not yet on a Raspberry Pi |

Build and runtime details: [ABIv11](verification-0.7.0.md),
[ABIv1 builds](verification-0.7.0-v1.md). The prebuilt SDK **0.5.0** targets
ABIv11 x86_64.

Macaros (hosted AArch64) needs a build of its own and has none. Other
architectures and other Amiga-family systems have no published Limpet build.
Each system needs its own binaries built with its SDK. An ABI match alone
does not establish compatibility with a particular system.

## Terminal versions

| Terminal | Presentation query | Stream features in this documentation |
| --- | --- | --- |
| Limpet 0.7.0 | answers, with input modes (including Shift+Enter), title feature, style, OSC 8 and icon bits | see the per-feature versions in the [catalogue](capabilities.md) |
| Limpet 0.6.0, 0.6.1 | answers, with input modes (including Shift+Enter) and title feature | as listed, except entries marked "Since 0.7.0" |
| Limpet 0.3.0–0.5.0 | answers, with input modes and title feature | as listed, except entries marked "Since 0.6.0" |
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

## Limits in 0.7.0

| Item | Limit |
| --- | --- |
| Text grid | 80 rows, 160 columns at most |
| Scrollback | 256 lines at 160 columns by default, 512 at 80 columns; set by `scrollback` (100 to 1000 lines at 160 columns, [settings](search-and-settings.md#settings)) |
| Command history | 32 commands by default; set by `history` (1 to 500) |
| Images | 16 per terminal, 1024x1024 pixels and 4 MiB each, 16 MiB together |
| Link targets | 2047 bytes, 64 remembered per terminal |
| Kitty control data | 512 bytes per command |

## Known behaviour that affects programs

- **No resize signal.** Ask for the size again, or (0.2.0) turn on resize
  reports in raw mode.
- **Input modes end with raw mode** (0.2.0). Set 2004, 2048, 2100, mouse
  tracking and (0.6.0) Shift+Enter again after each switch to raw mode.
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
  Limpet one. Limpet does not replace the system console. A program that
  takes a console name from you can be given `LIMPET:` instead, which is
  [experimental](limpet-handler.md).
- **`ioctl(TIOCGWINSZ)`** in a Limpet window crashed a POSIX program up to
  0.4.1 (the C library reads the console's size through a pointer Limpet
  left empty). From 0.5.0 it returns the window's rows and
  columns.
