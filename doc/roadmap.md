# Roadmap

Limpet 0.10.0 is available for AROS x86_64 ABIv11 and mainline x86_64
ABIv1, with an AArch64 Raspberry Pi test build. The experimental `LIMPET:`
handler is a separate download for all three targets; with `LimpetCON ON`
it can stand in for `CON:` until the system restarts.

New in 0.10.0: colour files made for Ghostty, Kitty and iTerm2 work as
themes, fifteen themes come with Limpet, and Preferences has pages for
appearance, the Shell and the terminal. See the [changelog](changes.md).

## Console compatibility

The handler already supports queued reads and timed waiters, cancellation,
Amiga cursor and window reports, Ctrl-C/D/E/F and Ctrl-\, and raw/cooked
line-feed behaviour. [The compatibility table](limpet-handler.md) records
what works and the remaining differences.

The optional, reversible replacement of `CON:` exists as an experiment.
Before it is offered for daily use: one font load shared by all console
windows, a round of programs started from Workbench with console windows,
and an optional line for `S:User-Startup`. Native Shell behaviour remains
part of that work, alongside full-screen terminal applications.

Shared interfaces are being proposed upstream:

- [Application capability query](https://github.com/aros-development-team/AROS/issues/1925).
- [Cancellation of a pending DOS read](https://github.com/aros-development-team/AROS/issues/1926).
- [Geometry query](https://github.com/aros-development-team/AROS/pull/1920)
  and [clipboard encoding](https://github.com/aros-development-team/AROS/pull/1909)
  are under discussion.

These are proposals, not accepted AROS interfaces. Limpet's existing private
contracts remain documented in the [protocol reference](protocols.md).

## Everyday use and terminal applications

Already released: command-history search, file drop, output search, font
and theme settings, Nerd Font icons and local-file links. Further work:

- Better completion for commands, paths and command arguments.
- Configurable shortcuts and prompt presentation.
- Horizontal and vertical panes with independent sessions.
- A distributable PTY/termios runtime for local terminal applications.
- Broader compatibility with editors, pagers and ncurses applications.
- SSH integration and, later, a multiplexer.

Helix already runs in Limpet as a separate AROS port. That does not imply
that every POSIX terminal application will run unchanged.

## Platforms

Continue building for mainline x86_64 and Raspberry Pi alongside ABIv11.
Use mainline facilities where available while retaining compatible paths
for older systems. The Pi build still needs testing on physical hardware.
Macaros work is parked; sharing AArch64 and ABIv1 does not establish binary
compatibility. i386 and other architectures need their own dependency,
build and runtime checks before a release is promised.

Mainline builds of 2026-10-24 to 2026-10-26 report support for translucent
windows but do not draw their frame, so Limpet cannot refuse them today. The
next step is to find the revision and library version that fixed the frame
and keep the solid background below it, rather than guessing from build dates.

## Graphics

Extend Kitty graphics beyond the current subset where applications need
it. Continue measuring scrolling and rendering costs before changing the
renderer. Demos remain useful examples, but do not hold up console work.
