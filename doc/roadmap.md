# Roadmap

Limpet 0.7.1 is available for AROS x86_64 ABIv11 and mainline x86_64
ABIv1, with an AArch64 Raspberry Pi test build. The experimental `LIMPET:`
handler is a separate download for all three targets. It does not replace
`CON:`.

## On main, not yet released

- Preferences for a command to run when a new Shell opens, including
  Fastfetch presets. See [session startup](session-start.md).
- SDK 0.6.0 queries for text support, terminal identity, Kitty graphics and
  pixel geometry.
- DEC mode 7 to turn automatic line wrapping on and off.

The [changelog](changes.md) separates these changes from released features.

## Console compatibility

The handler already supports queued reads and timed waiters, cancellation,
Amiga cursor and window reports, Ctrl-C/D/E/F and Ctrl-\, and raw/cooked
line-feed behaviour. [The compatibility table](limpet-handler.md) records
what works and the remaining differences, including `AUTO` and `WINDOW`.

The next step is to close those differences before offering optional,
reversible replacement of `CON:`. Native Shell behaviour remains part of
that work, alongside full-screen terminal applications.

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

## Graphics

Extend Kitty graphics beyond the current subset where applications need
it. Continue measuring scrolling and rendering costs before changing the
renderer. Demos remain useful examples, but do not hold up console work.
