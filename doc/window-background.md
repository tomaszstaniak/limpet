# Window background

Limpet can show what is behind its window through the terminal's background.
This is experimental and off until you choose it. Without it, and on systems
that cannot show it, Limpet looks and works as before.

## Choosing it

In a Limpet window, choose **Project > Preferences...**. Under **Window
background**:

- **Background opacity**: 100 % is a solid window, 20 % the most transparent.
- **Background blur**: blurs what is behind the window, so that text stays
  easy to read.

Press **Use** to keep the choice until the system restarts, **Save** to keep it
for good, or **Cancel** to leave everything as it was.

The window you opened Preferences from changes its opacity at once if it is
transparent already; other transparent windows do on Project > Reload settings.
A solid window, and a change of the blur, wait for the next window you open:
a window is made transparent when it opens.

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
something is not available; the option is then disabled and your saved choice
stays for a screen that has it. Started on its own, Preferences asks the
default public screen and says so.

| The screen can show | Limpet |
| --- | --- |
| transparent windows and blur | everything above |
| transparent windows | opacity; blur disabled |
| neither | a solid background |

Transparent windows need mainline AROS with the window compositor, built on
2026-10-27 or later, on a 32-bit screen. ABIv11 systems such as AROS One do
not have it. Blur needs a further system change that AROS does not include,
in the version this Limpet was written for; Limpet does not bring it along.

If the system runs short of memory for a window, the window opens solid, or
turns solid when it is made larger, and says so in its title bar.

## The settings file

Limpet Preferences writes `Window.conf` in `ENV:Limpet/` (Use) and
`ENVARC:Limpet/` (Save). Limpet reads the first one it finds.

```
background_opacity=70
background_blur=on
```

`background_opacity` takes 20 to 100 and `background_blur` takes `off` or `on`.
A file with any other line is not used as a whole, as with `Limpet.conf`, and
the title bar says why.
