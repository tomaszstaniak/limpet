# Roadmap after 0.1

Limpet 0.1 is available for AROS x86_64 ABIv11. The next releases will
bring it to more machines and make everyday Shell work more convenient.
The long-term goal is to replace `CON:` while keeping compatibility with
native AROS software.

Ports and new features are separate tracks. An unfinished port will not
hold up an update for an architecture already supported. These are
development priorities, not announced release dates or completed features.

## Next round

- Build for native AROS Raspberry Pi aarch64 using the existing toolchain
  and Developer SDK already used to compile and link FolioPDF. Check
  Limpet's dependencies on that target, then run and package the terminal.
- Reverse command-history search (Ctrl-R) is in 0.3.0, with the window
  title that names the running program and its state. File drag and drop
  is in 0.4.0. Scrollback search and a settings file are in the 0.5.0
  candidate.

## More architectures

Start with native AROS Raspberry Pi aarch64. The AROS-patched Clang 20.1.0
toolchain and Developer SDK from the 2026-08-22 system archive are already
available and have been used for FolioPDF builds. Reuse that configuration,
including its large code model, reserved x18 and non-PIC settings.
This establishes a build starting point, not runtime support for Limpet.
After that, investigate i386
ABIv11. Other targets, including m68k, need a dependency and platform
check before committing to a port.

For each target, check FreeType, HarfBuzz and utf8proc, pointer sizes,
DOS packet layout and endianness. A distributable port needs runtime
checks for Shell startup, Unicode, clipboard, reflow and Ctrl-C, followed
by a separate package and apkg entry. A successful build alone is not
a supported port.

Mainline packaging remains a separate task from ABIv11 ports.

## Everyday use

The intended order is:

1. Reverse command-history search with Ctrl-R (done in 0.3.0). It
   searches commands, not terminal output.
2. File drag and drop, inserting a correctly quoted path and handling
   Workbench icons separately from their associated files (done in 0.4.0).
3. Scrollback search for logs and command output (0.5.0 candidate).
4. Configuration of fonts, theme, scrollback and history limits (0.5.0
   candidate). Shortcuts are not configurable yet.
5. Better completion for commands and paths, followed by command arguments.
6. Horizontal and vertical panes with independent sessions.

Prompt configuration stays on the list. Shell prompt syntax and terminal
presentation should remain distinct.

## Replacing CON:

Turn the experimental console handler into a supported ABIv11 option.
Check programs that open console handles themselves, rather than only
programs launched inside an existing Limpet window. Installation must be
optional and reversible before offering a system-console replacement.

Stage 1 is done in the sources: [LIMPET:](limpet-handler.md) opens a Limpet
window for `NewShell WINDOW LIMPET:` and for programs that open
`LIMPET:...` themselves, with CON:'s name syntax, an install and removal
that last until the next reboot, and a compatibility table measured
against `CON:`. The same page lists what is missing before it could stand
in for `CON:`, and a design for that next step.

## Terminal applications and remote work

- Build and test the SSH client on ABIv11.
- Provide a distributable PTY/termios runtime path for local terminal apps.
- Check compatibility with real editors, pagers and ncurses applications.
- Work towards a multiplexer such as tmux or an AROS-native equivalent.
- Extend keyboard reporting, including modifiers and key releases.

Keep native Shell support, SSH and POSIX application support documented
separately so users know what is available on their system.

## Graphics and integrations

Extend Kitty graphics with compression, scaling and broader placement
support. Investigate redraw flicker and tearing using measurements before
adding a new rendering backend.

Editor integration and extension interfaces are later work. Define them
around useful integrations rather than a general plugin system upfront.

Doom, tracker interfaces, plasma and other terminal demos remain useful
experiments. Video, synth/audio extensions, Markdown rendering and an
animated MOTD or ASCII logo are longer-term ideas. They do not block
ordinary terminal improvements.
