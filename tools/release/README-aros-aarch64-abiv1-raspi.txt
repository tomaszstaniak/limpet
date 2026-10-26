Limpet @VERSION@ for AROS on Raspberry Pi (ABIv1, AArch64) -- test build
=========================================================================

Limpet is a terminal for the AROS Shell, written for AROS (it is not a port
of X11 xterm). Drawn with Intuition and FreeType:
- Unicode text: CJK, emoji, combining clusters;
- 16 and 24-bit colours and themes;
- selection, copy and paste;
- scrollback;
- the alternate screen used by full-screen programs;
- inline images through a documented subset of the Kitty graphics protocol.

Built for AROS on the Raspberry Pi (ABIv1, AArch64) with the Developer SDK
of the raspi-aarch64 system of 2026-08-22. This is a test build: it was run
under QEMU's Raspberry Pi 3 model, not yet on a real Raspberry Pi. Reports
from a real Pi are welcome. It is a build for the native Pi system; it
has not been checked on hosted AArch64 AROS (Macaros). x86_64 AROS needs
a build of its own.

Limpet does not replace the system console (CON:): programs that open
CON: themselves still get the system console window. An experimental
console handler, LIMPET:, comes as a separate archive and is not part of
this one.

Author: Tomasz Staniak, https://tomaszstaniak.com
Project: https://github.com/tomaszstaniak/limpet


About this build
----------------

This is the first Limpet build for this system. It has the same features
as Limpet @VERSION@ for ABIv11; the changes of each version are listed in
doc/changes.md in the source (https://github.com/tomaszstaniak/limpet).


Requirements
------------

- AROS for the Raspberry Pi (ABIv1, AArch64). Run so far on the system of
  2026-08-22, under QEMU only.
- The system font FONTS:TrueType/OpenSans-Regular.ttf, and
  FONTS:TrueType/VeraMono.ttf when the bundled font is removed or another
  font is set (both installed with the system).
- freetype2.library (part of the system).


Install
-------

The drawer is self-contained. Nothing is copied into the system.

The Pi system has no UnZip. Unpack
Limpet-@VERSION@-aros-aarch64-abiv1-raspi.zip on another computer and
copy the Limpet drawer and Limpet.info to the Pi's SD card, for example
into Utilities.

This creates SYS:Utilities/Limpet. Any other location works too; keep
the drawer together, because fonts are looked up in PROGDIR:Fonts.


Run
---

From Workbench: double-click the Limpet icon. A Shell starts in the window.

From a Shell:

    SYS:Utilities/Limpet/Limpet
    SYS:Utilities/Limpet/Limpet --shell
        a Shell in the terminal, with the usual Shell start-up script

    SYS:Utilities/Limpet/Limpet --shell --no-startup
        the same, without the start-up script

    SYS:Utilities/Limpet/Limpet --theme SYS:Utilities/Limpet/Themes/light.theme --shell
        a different colour theme (dark is built in; light and high-contrast
        are included)

End the Shell with EndCLI, then close the window, or choose
Project > Close window.


Settings
--------

Limpet reads ENV:Limpet/Limpet.conf when a window opens, or
ENVARC:Limpet/Limpet.conf when ENV: has none; without either it works as
before. One key=value per line, # starts a comment:

    font=FONTS:TrueType/VeraMono.ttf   a monospace TrueType/OpenType font;
                                       without it, Fonts/DejaVuSansMNerd-
                                       FontMono-Regular.ttf from the drawer
    font_size=13                       8 to 20 pixels
    theme=SYS:Utilities/Limpet/Themes/light.theme
    scrollback=256                     100 to 1000 lines at 160 columns
    history=32                         1 to 500 commands

A line Limpet does not understand makes it ignore the whole file; the
title bar names the line. Each 100 lines of scrollback take about 2.7 MB.
The theme changes at once with Project > Reload settings; font, size,
scrollback and history apply to windows opened afterwards. --theme on the
command line wins over the file.


Menu
----

    Project > About Limpet...
    Project > Reload settings
    Project > Close window      RAmiga+Q
    Edit > Copy                 RAmiga+C
    Edit > Paste                RAmiga+V
    Edit > Find in output       RAmiga+F


Keys
----

    Ctrl-C                    break the command running in the Shell
    Ctrl-D / Ctrl-E / Ctrl-F  send that break signal to the program
                              reading the window (Ctrl-D stops a script)
    Ctrl-\                    end input once (at the prompt: end the Shell)
    Up / Down                 earlier commands
    Ctrl-R                    search earlier commands (Enter: edit it,
                              Esc: back to the line as it was)
    TAB / Shift+TAB           complete a path or command
    Shift+PgUp / PgDn, wheel  scroll back through the output
    RAmiga+F / Ctrl+Shift+F   find text in the output (Up/Down: older and
                              newer, Enter: stay there, Esc: back)
    Ctrl+click on a link      open it (needs OpenURL); Ctrl+Shift+click
                              copies it
    drag icons onto the window  put their paths on the command line

    RAmiga+C / Ctrl+Shift+C   copy the selection
    RAmiga+V                  paste
    F5                        end the Shell's input (EOF); the Shell ends
    F4                        start the Shell again after it has ended


Features on this build
----------------------

    Shell in a window, Echo/$RC, redirection         yes
    Execute scripts, Run background jobs             yes
    Copy and paste, including into commands          yes
    Typing ahead during heavy output                 yes, in order, nothing lost
    Full-screen programs (alternate screen), resize  yes
    Width change keeps text and scrollback (reflow)  yes
    Unicode, CJK, emoji (monochrome)                 yes
    Kitty graphics, documented subset                yes
    Ctrl-C to break a running command                yes (see limitations)
    Selection kept while other rows are redrawn      yes (new in 0.2.0)
    Bracketed paste, resize and wheel reports        yes, for raw-mode programs
                                                     (new in 0.2.0)
    Window title: running command and its state      yes (new in 0.3.0)
    Ctrl-R search through the command history        yes (new in 0.3.0)
    Icons dropped from Workbench give their paths    yes, at the Shell prompt
                                                     (new in 0.4.0)
    Find in output                                   yes (new in 0.5.0)
    Settings file: font, theme, scrollback, history  yes (new in 0.5.0)
    Ctrl-D/E/F signals, Ctrl-\ end of input          yes (new in 0.6.0)
    Shift+Enter for raw-mode programs                yes, when the program
                                                     asks (new in 0.6.0)
    POSIX pseudo-terminal (PTY)                      no; the PTY is a separate
                                                     preview for mainline only


Known limitations
-----------------

- Ctrl-C goes to the Shell and the command it runs, not to jobs started
  with Run. The terminal sends the break only while the Shell is known to
  exist. On a multi-processor system, a break pressed at the exact moment
  the Shell is running on another processor is skipped; press Ctrl-C
  again. Programs that never check for a break cannot be stopped this way,
  as in the system console.
- The window title names the command the Shell runs. Jobs started with
  Run and processes a command starts are not shown. Characters outside
  Latin-1 in a program's title show as '?'.
- Ctrl-R searches only the commands typed in this window (the last 32,
  or as many as history= says); Find (RAmiga+F) searches the output.
- Find compares letters exactly, except A-Z with a-z; it has no patterns,
  and a full-screen program's own history is not in the window to search.
- Dropped icons go only to the Shell's own command line. A program that
  reads keys itself (an editor, a full-screen program) gets nothing, and
  neither do LimpetPTY sessions. Names that are not UTF-8 text (for
  example a Latin-1 "é") are refused, because Limpet sends commands to the
  Shell as UTF-8.
- A drop of a file name containing " is refused; drop those names
  separately.
- Commands that take patterns (List, Copy, Delete) still read #?()[]|~
  in a dropped name as pattern characters.
- F5 ends the input of the whole Shell session, not just of the running
  program.
- A Run whose output is redirected to a file leaves the file empty. This is
  a limitation of the system's Run, not of the terminal.
- Kitty graphics: raw RGB/RGBA only, no PNG, no compression, no animation
  frames; 16 images, 1024x1024 pixels each.
- The colour emoji font (Noto Color Emoji) is not included.
- After the window's width changes, a view scrolled back into the history
  returns to the live screen, and a selection is cleared. Text and
  scrollback are kept and rewrapped. Full-screen programs redraw
  themselves at the new size.
- Inline images placed above text follow it only approximately when the
  width changes; a height change keeps them on their row.


Uninstall
---------

    Delete SYS:Utilities/Limpet ALL
    Delete SYS:Utilities/Limpet.info



Limpet itself writes nothing else to the system: no preferences and no
assigns. A settings file you made stays; delete it with
    Delete ENVARC:Limpet ENV:Limpet ALL


Licences
--------

- Limpet, including its icon: MIT licence, LICENSE.txt.
- FreeType is not included; the system's freetype2.library is used.
- HarfBuzz 14.5.0: Licenses/HarfBuzz-COPYING.txt.
- utf8proc 2.11.0: Licenses/utf8proc-LICENSE.md.
- Unicode 17.0 character data (character width table), Unicode License
  v3: Licenses/Unicode-3.0.txt.
- Unifont 17.0.03 (Fonts/Unifont.otf), used under the SIL Open Font License
  1.1: Licenses/Unifont-OFL-1.1.txt, with the release's notices in
  Licenses/Unifont-COPYING.txt.
- Noto Emoji (Fonts/NotoEmoji.ttf), SIL Open Font License 1.1:
  Licenses/NotoEmoji-OFL.txt.

The fonts are unmodified.
