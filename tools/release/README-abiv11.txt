Limpet @VERSION@ for AROS ABIv11 (x86_64)
=========================================

Limpet is a terminal for the AROS Shell, written for AROS (it is not a port
of X11 xterm). Drawn with Intuition and FreeType:
- Unicode text: CJK, emoji, combining clusters;
- 16 and 24-bit colours and themes;
- selection, copy and paste;
- scrollback;
- the alternate screen used by full-screen programs;
- inline images through a documented subset of the Kitty graphics protocol.

Built for ABIv11 (x86_64) with the ABIv11 2026.09 SDK; this version was
tested on AROS One 1.3. Binaries for ABIv11 do not run on mainline AROS,
and mainline binaries do not run here.

Limpet does not replace the system console (CON:): programs that open
CON: themselves still get the system console window. An experimental
XTERM: console comes as a separate archive and is not part of this one.

Author: Tomasz Staniak, https://tomaszstaniak.com
Project: https://github.com/tomaszstaniak/limpet


New in 0.5.0
------------

- Fixed: a POSIX program that asked for the window size
  (ioctl TIOCGWINSZ, as programs built with the C library's terminal
  functions do) crashed in a Limpet window up to 0.4.1, with "Illegal
  address access" in crt.library. It now gets the window's rows and
  columns, also after a resize.
- Find in output: RAmiga+F or Ctrl+Shift+F (Edit > Find in output) finds
  text in the scrollback and on the screen. Type to search; Up or RAmiga+F
  again goes to an older match, Down to a newer one; Enter keeps the view
  there, Esc goes back. Output that arrives meanwhile does not move the
  view. A full-screen program's own screen is not searched.
- A settings file, ENV:Limpet/Limpet.conf (or ENVARC:Limpet/Limpet.conf):
  font, font_size, theme, scrollback and history; see "Settings" below.
  Project > Reload settings applies the theme at once.
- Several programs writing to the window at once wait their turn.


New in 0.4.1
------------

- Closing the window while a drop arrives no longer depends on how exec
  schedules interrupts: a small process answers drops while the window
  closes. (Not tested on an SMP system.)
- A program can take back a console read it still has out
  (XTY_ACTION_CANCEL, read only); hardcode uses it when it ends, so
  nothing it asked for is left for the Shell. Limpet 0.4.0 and earlier
  refuse it. Limpet SDK 0.3.0 is unchanged.
- The program is 3,386,296 bytes, 912 more than 0.4.0.


New in 0.4.0
------------

- Drag icons from a Workbench drawer or the desktop onto the window at the
  Shell prompt: their full paths go on the command line at the cursor,
  written as Shell arguments (quoted where needed, with *" ** *$ *` inside
  the quotes). Nothing runs until you press Enter. Several icons at once,
  drawers and disks work.
- A drop that cannot go in leaves the line as it was and the title bar
  says why: a program reading keys itself, a running command, an open
  history search, an icon without its object, a name that is not UTF-8.
- A drop that arrives while the window closes is answered at once, so the
  close never waits on Workbench.
- The program is 3,385,384 bytes, 9,560 more than 0.3.0 (+0.28 %).


New in 0.3.0
------------

- The window title says what runs in the window: "Shell - Limpet" at the
  prompt, "Wait - Limpet" while a command runs, and the program's own state
  when it reports one, e.g. "title demo: receiving - Limpet" from the SDK's
  title example. A program reports its state once it is built with Limpet
  SDK 0.3.0 (or sends OSC 0/2 itself after the presentation query).
  When the program ends the title shows "Shell" again. Messages such as
  "Copied" follow the name; key hints are in the About window.
- Ctrl-R searches the commands you typed (the last 32). Type to narrow,
  Ctrl-R again for an older one, Enter to edit it (it is not run), Esc to
  go back to what you had.
- Ctrl-C while a command runs gives one new prompt instead of two.
- An image near the bottom of the window shows whole: the text scrolls up
  to make room. The clear command removes images with the text.
- Images stay on their text row when the window is resized.
- For programs: OSC 0 and OSC 2 set the window title, announced by the
  presentation query; Limpet SDK 0.3.0 has limpet_osc_title().
- The program is 3,375,824 bytes, 18,288 more than 0.2.1 (+0.54 %).


New in 0.2.1
------------

- HarfBuzz 14.5.0 instead of 11.2.1: a maintenance update with upstream
  fixes in shaping and in handling damaged fonts. No new terminal
  features; text draws as in 0.2.0. With a font whose tables are damaged,
  a cluster may now fall back to another font or lose a ligature.
- The program is 4.95 % larger (3,357,536 bytes instead of 3,199,064).


New in 0.2.0
------------

- A selection stays while a program redraws other rows (a status line, a
  clock, a streamed reply below it). Output that changes or scrolls the
  selected text still clears it.
- For full-screen programs in raw mode, and announced by the presentation
  query so a program can check for them first:
  - bracketed paste (DEC mode 2004): the paste arrives framed, without the
    multi-line confirmation, and nothing else is inserted into it;
  - resize reports (DEC mode 2048) in the input;
  - wheel reports without mouse tracking (Limpet's private mode 2100), so
    a plain drag still selects text.
  When the console returns to cooked mode, these modes and mouse tracking
  are turned off, so the Shell and the next program start clean.
- Erasing a whole row no longer joins it to the row above when the width
  changes later (a redrawn status panel no longer leaves joined lines in
  the history).


Requirements
------------

- AROS ABIv11, x86_64. This version was tested on AROS One 1.3.
- The system fonts FONTS:TrueType/VeraMono.ttf and
  FONTS:TrueType/OpenSans-Regular.ttf (installed with the system).
- freetype2.library (part of the system).


Install
-------

The drawer is self-contained. Nothing is copied into the system.

    apkg install limpet

(apkg puts it in its packages drawer and removes it with apkg remove), or
by hand:

    UnZip Limpet-@VERSION@-abiv11.zip -d SYS:Utilities

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

    font=FONTS:TrueType/VeraMono.ttf   a monospace TrueType/OpenType font
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
- With a file name containing " on the line, the AROS Shell miscounts
  quotes, and a ; later inside quotes would end the command. Limpet
  refuses such a drop; drop those names separately.
- Commands that take patterns (List, Copy, Delete) still read #?()[]|~
  in a dropped name as pattern characters.
- F5 ends the input of the whole Shell session, not just of the running
  program.
- Memory: each terminal session leaves about 0.2 KB that is not returned
  (measured over 100 sessions). About 0.12 KB of that is what the system
  keeps for any Shell's life. The remaining 70-75 B is not explained yet.
  It matters only after thousands of sessions without a reboot.
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

(after apkg install: apkg remove limpet)

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
