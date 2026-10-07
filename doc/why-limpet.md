# Why Limpet

## What Limpet is

The **AROS Shell** is the command interpreter: it reads your commands, runs
programs and scripts, and prints the prompt. It needs a console to run in.
Normally that is `CON:`, the system console window.

**Limpet is a different console for the same Shell.** It is a terminal
window written for AROS, and the standard AROS Shell runs in it unchanged:
the same commands, scripts, assigns and prompt. Nothing about the Shell
changes. What changes is how its output is shown and what programs running
in it can do.

So the comparison on this page is between the Shell in `CON:` and the Shell
in Limpet. Programs that open `CON:` themselves still get a `CON:` window;
Limpet does not replace the system console.

## What you notice as a user

**Text in any language.** Limpet decodes UTF-8: Polish, Czech, Greek,
Cyrillic, Chinese, Japanese and Korean text, combining accents and
monochrome emoji. `CON:` shows the same bytes as Latin-1 characters.

**Long output you can go back to.** Command output goes into a scrollback
of 256 lines at full width, more when the window is narrower (512 at 80
columns). When you make the window
narrower or wider, Limpet rewraps the text and the history instead of
cutting lines off.

**Copy and paste that works with commands.** Select text with the mouse,
copy with RAmiga+C and paste with RAmiga+V. Limpet uses the system
clipboard, and pasting works in the command line you are typing.

**Links you can open.** Addresses in output are recognised. Hover to see
the target in the title bar, Ctrl+click to open it with OpenURL, or
Ctrl+Shift+click to copy it. Programs can also print a short label that
links to a long address.

**Colour where the program asks for it.** Tools that know about Limpet use
16, 256 or 24-bit colour, and update progress bars in one line. Your Shell
prompt keeps its Workbench pen colours, as in `CON:`.

**Pictures next to text.** Programs can draw images in the window through a
subset of the Kitty graphics protocol.

**A selection that stays put** (0.2.0). Select a few lines while a program
keeps updating its status line or streams a reply below them: the
selection stays, and RAmiga+C copies it. It goes away only when the
selected text itself changes or scrolls.

## What you get as a programmer

**You can ask before you decorate.** One DOS packet on your output handle
tells you whether colour, bold and in-place line updates will be shown, and
how big the window is. It does not read the keyboard, it does not change
modes, and in `CON:` it fails at once with `ERROR_ACTION_NOT_KNOWN`. A
program can therefore use colour in Limpet and stay plain in `CON:` and in
files, with no guessing and no configuration.

This is what apkg, the AROS package manager, does. In Limpet it shows
coloured verdicts, a table sized to the window and a single-line download
bar. In `CON:` and in redirected output it prints the same information as
plain text.

| apkg in Limpet | SDK examples in `CON:` |
| --- | --- |
| ![apkg show in Limpet, dark theme](screenshots/apkg-show-dark.png) | ![SDK examples falling back to plain text in CON:](screenshots/sdk-con-fallback.png) |

*Left: apkg in Limpet 0.1.0. Right: the SDK examples in `CON:` on the same
system print complete plain text.*

**Full-screen programs that behave like the user expects** (0.2.0). A
program in raw mode can ask for pastes as one framed block (so a pasted
newline is not Enter), for size changes as input, and for wheel steps
without taking over the mouse, so the user can still select and copy.
Limpet turns these modes off when the program returns the console to
cooked mode, so the Shell never inherits them.

hardcode, an AI coding agent for AROS, uses all three: a 50-line paste
lands in its input box as text, its panel redraws at every width, and a
selection survives while the reply streams in.

| A 50-line paste in hardcode's input box | Selection kept while a reply streams |
| --- | --- |
| ![hardcode in Limpet with a 51-line draft after a paste](screenshots/hardcode-paste.png) | ![hardcode in Limpet: a selected line stays while the reply grows below](screenshots/hardcode-selection-stream.png) |

*hardcode 0.3.0-dev in Limpet 0.2.0 on AROS ABIv11 2026.09 under QEMU,
against a local test server (no AI model), 2026-10-07.*

**Standard sequences, not a private dialect.** Output uses well-known
protocols: ANSI/VT colour and cursor sequences, OSC 8 links and Kitty
graphics. A program written for these elsewhere needs little change. The
Limpet-specific part is the detection packet, the pens rule and one
private mode (2100, wheel reports).

## What is specific to Limpet

Several terminals implement the same standard sequences. What Limpet adds
on AROS:

- it runs the **native AROS Shell** in a native Intuition window, not a
  Unix shell in an emulation layer;
- it keeps **AROS console conventions**: Workbench pens for SGR 30-37,
  Ctrl-C as the break signal, `WaitForChar()` and raw mode as programs
  expect, and from 0.2.0 it ties input modes to raw mode, which ends them
  when the program hands the console back;
- it uses **AROS services**: the system clipboard, OpenURL, FreeType and
  the system fonts;
- it offers **detection through a DOS packet**, which needs no reply on
  the input stream and therefore never puts stray characters into the
  Shell.

The standard protocols themselves (VT sequences, OSC 8, Kitty graphics) are
not Limpet's invention, and Limpet implements documented subsets of them.

## Side by side

The same test program, sending the same bytes without asking first, in
both consoles. System console: `CON:` of AROS ABIv11 2026.09 x86_64,
Limpet 0.1.0 on the same system, both under QEMU, 2026-10-05.

| Bytes sent | `CON:` | Limpet 0.1.0 |
| --- | --- | --- |
| UTF-8 text with Polish letters, CJK and an emoji | shown as Latin-1 characters | shown correctly |
| `ESC[31m` | pen 1 | pen 1 (same as `CON:`) |
| `ESC[38;5;196m` (256 colours) | ignored, default colour | bright red |
| `ESC[38;2;255;128;0m` (24-bit) | ignored, default colour | orange |
| OSC 8 link | sequence printed as text | label shown as a link |
| CR, then `ESC[K` | line rewritten | line rewritten |
| presentation query (DOS packet) | `ERROR_ACTION_NOT_KNOWN` | colour level, pens, size |
| cursor keys in raw mode | `0x9B A` (8-bit CSI) | `ESC [ A` |

| `CON:` | Limpet |
| --- | --- |
| ![Comparison probe in CON:](screenshots/sdk-con-compare.png) | ![Comparison probe in Limpet](screenshots/sdk-limpet-compare.png) |

Only these rows were tested. Other `CON:` behaviour is not described here.

## What Limpet does not do yet

- It is for AROS ABIv11 x86_64 only. Mainline and other architectures are
  on the roadmap, not released.
- It does not replace `CON:` system-wide.
- POSIX programs that need termios (most Unix TUI programs) do not run in
  it yet.
- There is no resize signal. Raw-mode programs can get resize reports
  (0.2.0); others ask for the size again.
- Images are raw RGB/RGBA only, without PNG or scaling.

The [capability catalogue](capabilities.md) lists every feature with its
status.
