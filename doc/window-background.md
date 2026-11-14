# Window background

Limpet can show what is behind its window through the terminal's background.
This is experimental and off until you choose it. Without it, and on systems
that cannot show it, Limpet looks and works as before.

## Choosing it

In a Limpet window, choose **Project > Preferences...**. Under **Window
background**:

**Background opacity**: 100 % is a solid window, 20 % the most transparent.

**Background blur** is shown disabled: this version of Limpet has no blur.

Press **Use** to keep the choice until the system restarts, **Save** to keep it
for good, or **Cancel** to leave everything as it was.

The window you opened Preferences from changes its opacity at once if it is
transparent already; other transparent windows do on Project > Reload settings.
A solid window waits for the next window you open: a window is made
transparent when it opens.

## What changes

Only the terminal's default background changes. Text, the cursor, the
selection, search marks, pictures and any background colour a program sets
stay solid, and text stays as sharp as in a solid window. So a full-screen
program with a theme of its own, such as Helix with its default theme, looks
as it does in a solid window. Choose a theme without a background of its own
(in Helix, `base16_transparent`) to see through it.

Clicks and text selection work on the transparent background as on a solid
one.

## What it needs

Preferences asks the screen of the window you opened it from and says when
transparency is not available; the option is then disabled and your saved
choice stays for a screen that has it. Started on its own, Preferences asks the
default public screen and says so.

Transparent windows need mainline AROS with layers.library 50.9 or newer
(builds from the evening of 2026-10-27 on), on a 32-bit screen; on the
builds of the three days before, which show such windows without a frame,
Limpet keeps the background solid since 0.10.1. This includes the Raspberry Pi system. ABIv11 systems such as AROS One do not have them: there the
background stays solid and the saved setting is kept.

If the system runs short of memory for a window, the window opens solid, or
turns solid when it is made larger, and says so in its title bar.

## The settings file

Limpet Preferences writes `Window.conf` in `ENV:Limpet/` (Use) and
`ENVARC:Limpet/` (Save). Limpet reads the first one it finds.

```
background_opacity=70
```

`background_opacity` takes 20 to 100. A `background_blur` line, `off` or `on`,
is accepted and kept, and has no effect.
A file with any other line is not used as a whole, as with `Limpet.conf`, and
the title bar says why.
