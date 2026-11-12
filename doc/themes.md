# Themes

A theme sets the terminal's default text and background colours, the 16
palette colours, the cursor and the selection. Colours a program asks for
itself (24-bit colours, palette entries 16 to 255) are never changed by a
theme, so a full-screen program with colours of its own, such as an editor
with its own colour scheme, looks as it does elsewhere.

## Choosing a theme

Project > Preferences, Appearance. The list shows the themes that come with
Limpet and the files in your themes drawer. Choosing one shows its colours
in Preferences and, at once, in the terminal window Preferences was opened
from. **Use** keeps it until the next restart of the system, **Save** for
good, **Cancel** returns the window to the theme it had.

Themes that come with Limpet:

- Limpet's own: the built-in dark theme, `dark`, `light`, `high-contrast`,
  `magicwb` (the MagicWB palette) and `amigaos31` (the four default
  colours of Workbench 3.1).
- Popular schemes: `dracula`, `solarized-dark`, `solarized-light`,
  `catppuccin-mocha`, `catppuccin-latte`, `nord`, `gruvbox-dark`,
  `tokyo-night`, `one-dark` and `rose-pine`. Their colours are their
  authors', used under their licences (`Licenses/Themes-LICENSES.txt` in
  Limpet's drawer); each file names its source.

For any other scheme, see the next section.

## Using a colour file made for another terminal

Limpet reads these files as they are:

| Made for | Usual name | Notes |
| --- | --- | --- |
| Ghostty | no ending | `palette = 0=#rrggbb`, `background = ...` |
| Kitty | `.conf` | `color0 #rrggbb`, `background ...` |
| iTerm2 | `.itermcolors` | the XML colour preset; colours in Display P3 are converted |
| Limpet | `.theme` | see below |

1. Download the colour file of the scheme you want, in one of these
   formats. Collections of terminal colour schemes usually offer each
   scheme in all three.
2. In Preferences, press **Open themes drawer** and put the file into the
   drawer that opens (`ENVARC:Limpet/Themes`). Or press **Add file...** and
   choose the file where it is; Limpet copies it there.
3. Back in Preferences the file is in the list, marked `(yours)`. Choose it.

The file's name does not matter: Limpet recognises the format by what is in
the file. Only colours are read. Anything else in the file (fonts, key
bindings, window settings) is skipped, and nothing in it is ever included
from elsewhere or run; the title bar says how many settings were skipped. A
whole `kitty.conf` or Ghostty configuration works as a theme if it sets
colours.

A file Limpet cannot use is not chosen, and Preferences (or the title bar)
says why: not a Ghostty, Kitty or iTerm2 colour file, a binary property
list (export the preset from iTerm2 as XML), no colours in it, or a line
whose value is not a colour. Files may be up to 64 KB.

What a file leaves out is filled in: the bright colours from the normal
ones, the cursor from the text colour, the selection as inverse video.
Other terminals' formats that are not read: Alacritty, Windows Terminal,
Xresources.

## Without Preferences

`theme=` in `ENV:Limpet/Limpet.conf` (see [settings](search-and-settings.md#settings))
and the `--theme` option take a path, or the name of a file in
`ENVARC:Limpet/Themes` or in the `Themes` drawer of Limpet. The endings
`.theme`, `.conf` and `.itermcolors` may be left out:

    theme=magicwb
    theme=Dracula
    theme=Work:Downloads/Solarized Dark.itermcolors

Project > Reload settings applies a changed theme. A theme that cannot be
used leaves the window as it is and the title bar says why; when a window
opens with such a theme it uses the built-in dark one.

## Bold text

Limpet shows bold text in the palette's first eight colours with the bright
version of that colour. Some schemes are not made for that: their bright
colours are different colours, not brighter ones. Preferences, Appearance,
"Bold text uses the bright colours" turns it off. The setting is
`bold_bright=off` in `ENV:Limpet/Colours.conf`.

## The Shell's prompt

The AROS Shell colours its prompt with the Workbench pens, not with the 16
palette colours. Limpet takes pens 0 and 1 from the theme's background and
text colours and the others from the palette, and keeps them readable when
a scheme's palette would not: the prompt's accent is never the text colour
itself, and a pen too close to the background is shown in the text colour.

## Writing a theme

Limpet's own format is one `key=#RRGGBB` per line; `#` starts a comment.
Keys: `foreground`, `background`, `cursor`, `selection_fg`, `selection_bg`,
`ansi0` to `ansi15`, and `pen2` to `pen7` for the Workbench pens (pens 0 and
1 are the background and the text colour). Keys left out keep the built-in
dark theme's value. This format is read strictly: an unknown key or a
value that is not `#RRGGBB` refuses the file. At most 4096 bytes. The
files in Limpet's `Themes` drawer are examples.

## Programs that ask for the colours

A program can ask what the colours are and choose its own to match:
`OSC 10 ; ?`, `OSC 11 ; ?` and `OSC 12 ; ?` for the text, background and
cursor colours, `OSC 4 ; N ; ?` for palette entries 0 to 15, each answered
as `rgb:RRRR/GGGG/BBBB`; `CSI ? 996 n` is answered `CSI ? 997 ; 1 n` for a
dark background and `; 2 n` for a light one, and with mode 2031 set the
same report is sent when a change of theme crosses between dark and light.
Setting colours through these sequences is not supported.
