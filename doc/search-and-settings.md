# Search and settings

Limpet 0.5.0 adds output search and a configuration file for the font, theme
and history limits.

## Find in output

Find searches the screen and scrollback. Ctrl-R searches command history
separately.

| Key | What it does |
| --- | --- |
| RAmiga+F, Ctrl+Shift+F, or Edit > Find in output | opens Find |
| typing | the text to find; the newest match at or above the bottom of the view is shown |
| Up, or RAmiga+F again | the next older match |
| Down | the next newer match |
| Backspace | removes the last character and searches again from where the view was |
| Enter | closes Find and keeps the view where the match is |
| Esc | closes Find and goes back to the view from before |
| Shift+PgUp/PgDn, the wheel | scroll while Find is open |
| RAmiga+V, Ctrl+Shift+V | pastes the clipboard's first line into the search text |
| Ctrl-C | closes Find, then interrupts the command as always |

The title bar shows the search text and where you are: `Find: error - 3 of 17`,
`Find: error - not found`. `(oldest)` and `(newest)` say that Up or Down has
nowhere further to go. The match shown is in the selection colours; other
matches in view are underlined.

Matching rules:

- literal text, without patterns or regular expressions;
- ASCII letters are matched case-insensitively; other letters must match exactly
  (`Ż` does not find `ż`);
- whole characters only: a search never takes half of a wide character or
  part of a combined one (`e` does not find `é`);
- characters are not normalised: a precomposed `é` and `e` followed by a
  combining accent are different text;
- a line that wrapped because it was too long for the window is one line;
  a real line end is never crossed.

While Find is open:

- Incoming output is buffered without moving the view. Match counts update
  as text arrives. Esc restores the previous view, including the bottom
  position if that was selected.
- Typing goes to the search field. Previously queued input and a paste
  already in progress wait until Find closes.
- Resizing preserves the selected match, indexed from the newest result,
  while text is rewrapped.
- When old text leaves the scrollback, its matches are removed and Find
  selects the next available match.

Full-screen programs: while a program uses the alternate screen (an editor,
a pager, hardcode's full-screen view), Find says so in the title bar and
does not open. What such a program scrolled away is kept only by that
program, so Limpet cannot search it; use the program's own search. If a
program switches to the alternate screen while Find is open, Find closes.

Find does not save anything to disk.

## Settings

Limpet reads `ENV:Limpet/Limpet.conf` when a window opens, or
`ENVARC:Limpet/Limpet.conf` if there is none in `ENV:`. Without either file, Limpet uses its defaults. `ENVARC:` keeps the file over a reboot; the system
copies it to `ENV:` at the next start. To use a change now, put the file in
both (or in `ENV:` only, to try it until the next reboot).

    MakeDir ENVARC:Limpet ENV:Limpet
    ; write ENVARC:Limpet/Limpet.conf with any text editor, then
    Copy ENVARC:Limpet/Limpet.conf ENV:Limpet/

An example:

    # Limpet settings: one key=value per line.
    font=FONTS:TrueType/DejaVuSansMono.ttf
    font_size=15
    theme=SYS:Utilities/Limpet/Themes/light.theme
    scrollback=500
    history=100

| Key | Value | Default | Takes effect |
| --- | --- | --- | --- |
| `font` | path of a monospace TrueType or OpenType font | DejaVu Sans Mono Nerd Font (`PROGDIR:Fonts/DejaVuSansMNerdFontMono-Regular.ttf`) when present, else Vera Mono (`FONTS:TrueType/VeraMono.ttf`) | in new windows |
| `font_size` | 8 to 20 pixels | 13 | in new windows |
| `theme` | path of a theme file (see the Themes drawer) | the built-in dark theme | at once with Project > Reload settings, and in new windows |
| `scrollback` | 100 to 1000 lines | 256 | in new windows |
| `history` | 1 to 500 commands | 32 | in new windows |
| `session_start` and the other `session_` and `fastfetch` keys | see [A command for every new Shell window](session-start.md) | `off` | in new windows |

- **`scrollback`** is the number of lines kept at the widest window, 160
  columns. A narrower window keeps more of its shorter lines: 512 lines at
  80 columns with the default. Each line costs about 27 KB of memory (the
  text and a second copy that rewrapping needs), so 1000 lines take about
  27 MB per window; the default takes about 7 MB, as before.
- **`history`** is how many commands Up, Down and Ctrl-R remember in a
  window, about 1 KB each. It is not saved when the window closes.
- **`font`** replaces the main text font. Emoji, CJK and other characters
  it lacks still come from the fonts in Limpet's drawer. The cell must stay
  within 32 by 32 pixels.
- **`theme`**: the `--theme` option on the command line wins over the file.

Configuration is validated as a whole. An unknown key, a duplicate key, a
number out of range or a line without `=` makes Limpet ignore the file and
use its built-in settings; the title bar says which line is wrong, for
example `Settings: Limpet.conf line 2: scrollback takes 100 to 1000;
built-in settings used`. A font that cannot be used (missing, damaged, not
monospace, too large for a cell) falls back to the default font, and a theme that
cannot be read to the built-in dark theme; both say so in the title bar.
If there is not enough memory for the scrollback asked for, Limpet uses the
default and says so.

Leading and trailing spaces around values are ignored, so paths with spaces inside work
without quotes. Lines starting with `#` are comments. The file may be at
most 4096 bytes.

Project > Reload settings reads the file again. The theme changes at once;
the title bar says when font, size, scrollback or history differ, which
apply to windows opened after that. A file that is wrong is ignored, the
window keeps what it has, and the title bar names the line and says
`nothing changed`.

Windows of the experimental `LIMPET:` handler read the same file when they
open; they have no Reload settings (see [LIMPET:](limpet-handler.md)).
