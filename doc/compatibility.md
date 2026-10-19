# Compatibility and limits

## Systems

The current terminal release, **0.6.1**, and the prebuilt SDK **0.4.0**
target **AROS ABIv11 x86_64**. The 0.6.1 archive was checked on AROS One 1.3;
see the [verification record](verification-0.6.1.md) for build and runtime details.

Mainline x86_64 has an older development preview, but no current public
release. Other architectures and other Amiga-family systems have no published
Limpet build. ABIv11 and mainline require separate binaries and matching SDKs.
An ABI match alone does not establish compatibility with a particular system.

## Terminal versions

| Terminal | Presentation query | Stream features in this documentation |
| --- | --- | --- |
| Limpet 0.6.0, 0.6.1 | answers, with input modes (including Shift+Enter) and title feature | see the per-feature versions in the [catalogue](capabilities.md) |
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

## Limits in 0.6.1

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
