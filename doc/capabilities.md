# Capability catalogue

This is the single list of what Limpet does, for users and for programmers.
Each entry says how a program finds out about it, what to send, what to do
when it is missing, and how far it has been checked.

Applies to **Limpet 0.1.0** for **AROS ABIv11, x86_64** (the only public
build) and **Limpet SDK 0.1.0**. Other builds are covered in
[compatibility](compatibility.md).

## Status labels

| Label | Meaning |
| --- | --- |
| **Verified** | In the 0.1.0 release and checked on that build running on AROS ABIv11 2026.09 x86_64 (QEMU). |
| **Released, host-tested** | In the 0.1.0 release. Its exact bytes are checked by host tests; on AROS it was checked on a development build or a different system, as stated. |
| **Experimental** | In the source tree only, not in any public release. May change or disappear. |
| **Not supported** | Not implemented. A program must not rely on it. |

"Host tests" are the programs under `tests/` and `sdk/tests/`, run by
`make test` on a desktop computer. They prove the terminal model and the
SDK produce and accept the right bytes. They prove nothing about AROS on
their own.

## How a program finds out

There are three separate ways, and most capabilities have only one of them:

1. **The presentation query** (operation 13). A DOS packet on your output
   handle. It reports colours, bold, line rewriting, the window size and
   how SGR 30-37 are read. It does not read input. This is the method to
   use first. [Details](presentation.md).
2. **Stream queries** such as `CSI 6 n` or the Kitty `a=q` query. The
   answer arrives as input, and Limpet sends it only while the console is
   in raw mode. Use them only after the presentation query has succeeded.
3. **Nothing.** Many features (Unicode, scrollback, the alternate screen)
   have no query. A successful presentation query means the console
   implements this protocol; today that is Limpet 0.1.0 or later, and this
   catalogue says what that version does.

`XTY_CAPS` (operation 1) is **not** a way to find any of this: a Limpet
0.1.0 Shell session does not answer it. See
[the protocol reference](protocols.md#control-operations).

## Summary

| Capability | For | How to detect | Status |
| --- | --- | --- | --- |
| [Presentation query](#presentation-query) | programs | itself | Verified |
| [16 colours](#colours) | programs | query, colour level 1+ | Verified |
| [256 colours](#colours) | programs | query, colour level 2+ | Verified |
| [24-bit colour](#colours) | programs | query, colour level 3 | Verified |
| [SGR 30-37 as Workbench pens](#sgr-30-37-as-workbench-pens) | programs | query, `XTY_COLOR_PENS` | Verified |
| [Bold and reset](#text-styles) | programs | query, `XTY_STYLE_BOLD_RESET` | Verified |
| [Underline, inverse](#text-styles) | programs | none | Released, host-tested |
| [Rewriting a line](#rewriting-a-line) | programs | query, CR and ERASE_EOL | Verified |
| [Window size](#window-size) | programs | query, geometry known bit | Verified |
| [Size by `CSI 18 t`](#window-size) | raw-mode programs | none | Released, host-tested |
| [Cursor movement and erasing](#cursor-movement-and-erasing) | full-screen programs | none | Released, host-tested |
| [Cursor position report](#cursor-position-report) | raw-mode programs | none | Released, host-tested |
| [Alternate screen](#alternate-screen) | full-screen programs | none | Verified |
| [Unicode text](#unicode-text) | everyone | none | Verified |
| [Hyperlinks (OSC 8)](#hyperlinks) | programs | query success | Verified (display, hover); opening not verified |
| [Plain URL detection](#hyperlinks) | users | none | Released, host-tested |
| [Inline images (Kitty subset)](#inline-images) | programs | query success, then `a=q` | Verified |
| [Keyboard input, cooked and raw](#keyboard-input) | programs | none | Verified |
| [Ctrl-C](#ctrl-c) | everyone | none | Verified, with limits |
| [Mouse reporting](#mouse-reporting) | raw-mode programs | none | Released, host-tested |
| [Scrollback, reflow, selection, clipboard](#user-interface-features) | users | not visible to programs | Verified |
| [Themes](#user-interface-features) | users | not visible to programs | Verified |
| [termios, PTY, TTY control operations](#terminal-settings-termios-and-pty) | POSIX programs | `XTY_CAPS` on a PTY | Experimental |
| [SSH client](#ssh) | users | none | Experimental |
| Window title (OSC 0/2), save cursor (DECSC), bracketed paste, any-motion mouse, resize events | | | Not supported |

The rest of this page has one section per row.

---

## Presentation query

**What it gives you.** One answer to "what will this console render?",
without touching the keyboard or terminal modes: colour level, bold and
reset, line rewriting, current size, and how SGR 30-37 are read.

**Problem it solves.** On AROS the same program can write to `CON:`, to a
file, to a pipe or to Limpet. Escape sequences that look good in Limpet
print as garbage in `CON:` and end up in files. The query lets a program
decorate only when the decoration will be shown.

**Detect.** It is its own detection. `ERROR_ACTION_NOT_KNOWN` means the
console does not implement it.

**Use.**

```c
LimpetPresentation p;
fflush(stdout);
if (limpet_query_presentation(Output(), &p, NULL) == LIMPET_OK) {
    /* decorate according to p */
} else {
    /* plain text */
}
```

Raw protocol: operation 13 in an `XtyControl` block, sent as packet type
`XTY_ACTION_CONTROL`. Wire format in [presentation.md](presentation.md);
API in [the SDK reference](sdk/api.md).

**Rules that matter.**

- **Query the handle you write to.** Usually `Output()`. Never `Input()`:
  with `program >file` the input is still the terminal but the text goes to
  the file.
- **A clear known bit means unknown, not absent.** A set bit with a zero
  value means confirmed absent. The SDK zeroes fields whose bit is clear.
- **Ignore what you do not know.** Unknown bits and the reserved values
  7 to 11 must not make a client fall back to plain text. A newer Limpet
  may set them.
- **The answer is a snapshot.** It describes the console at that moment.
  The size in particular changes when the user resizes the window.
- **It blocks** until the handler replies. Limpet and `CON:` reply at once.
  There is no timeout: DOS packets have none.
- **Non-interactive handles get no packet.** The SDK checks
  `IsInteractive()` first and returns `LIMPET_NOT_INTERACTIVE`.
- **`ERROR_ACTION_NOT_KNOWN`** comes from `CON:`, from consoles that are
  not Limpet, and from a Limpet session that is closing. All three mean
  "write plain text".

**Example.** [`hello`](../sdk/examples/hello.c).

![Output of the hello example in Limpet](screenshots/sdk-hello.png)

*`hello` in Limpet 0.1.0 on ABIv11 2026.09: 24-bit colour, pens, bold,
line rewriting, 117x37 cells.*

**Status.** Verified 2026-10-05: Limpet 0.1.0 (release archive SHA-256
`668ba57e...a339a0`) on AROS ABIv11 2026.09 x86_64 under QEMU answers with
known mask 31, colour level 3, `XTY_COLOR_PENS`, and the live size. On the
same system `CON:` answers `ERROR_ACTION_NOT_KNOWN`, and output redirected
to a file is reported as not interactive. Earlier, apkg's presentation
support was accepted against this query on AROS One 1.3.
Host test: `sdk/tests/test_sdk.c` sends the SDK's request through Limpet's
own reply code.

---

## Colours

**What it gives you.** Coloured text at one of three levels.

| Level | Value | Promised sequences (7-bit `ESC [` only, semicolon form) |
| --- | --- | --- |
| ANSI16 | 1 | SGR 30-37, 90-97, 39 (foreground); 40-47, 100-107, 49 (background) |
| INDEXED256 | 2 | adds `38;5;n` and `48;5;n` |
| RGB | 3 | adds `38;2;r;g;b` and `48;2;r;g;b` |

Limpet 0.1.0 reports level 3. Colon forms (`38:2::r:g:b`) are not
promised. The 8-bit CSI byte 0x9B is not promised.

**Detect.** Presentation query: known bit `XTY_PRESENT_KNOWN_COLORS`, value
in `color_level`. Use `limpet_color_level()`, which treats a level newer
than the SDK as unknown.

**Use.** `limpet_sgr_ansi()` for the 16 colours, `limpet_sgr_rgb()` for
24-bit colour with automatic reduction to the 256-colour cube. End colours
with SGR 39/49 or SGR 0.

**Fallback.** No colour at all. Every coloured word must also make sense as
plain text: write "error:", not just red. Read the
[pens section](#sgr-30-37-as-workbench-pens) before writing SGR 30-37.

**Example.** [`colors`](../sdk/examples/colors.c).

![The colors example in Limpet: 16 colours with samples and swatches, a 256-colour ramp and a 24-bit gradient](screenshots/sdk-colors.png)

*`colors` in Limpet 0.1.0, dark theme. Colour 0 is black text on a dark
background, which is why the example prints names in the default colour.*

**Status.** Verified (all three levels, `colors` example, 2026-10-05).
Host tests: `tests/test_screen_model.c`, `tests/test_theme.c`.

---

## SGR 30-37 as Workbench pens

**What it gives you.** The AROS Shell prompt and other native programs
look the same in Limpet as in `CON:`, because Limpet reads SGR 30-37 and
40-47 as **Amiga screen pens 0-7**, exactly like the system console. Pen 1
is not red; it is whatever the Workbench palette says.

**Problem it solves.** Native programs written for `CON:` keep their
colours. The cost falls on programs that mean ANSI red when they write
`ESC [31m`.

**Detect.** Presentation query: `XTY_COLOR_PENS` in `interpretation`,
valid only when the known bit `XTY_PRESENT_KNOWN_INTERPRETATION` is set.
`limpet_uses_pens()` checks both.

**Use.** When pens are on, send the 16 ANSI colours as `38;5;0` to
`38;5;15` (`48;5;n` for background). These keep their ANSI meaning and are
guaranteed from colour level ANSI16 up. `limpet_sgr_ansi()` does this
choice for you.

**Reset.** SGR 39 restores the theme's default foreground and drops a
foreground pen. It does not bring back an earlier pen, and it leaves
background and bold alone (SGR 49 likewise). The next Shell prompt sets its
own pens, so a program only has to end its own colours.

**Fallback.** When the interpretation is unknown, keep normal ANSI
behaviour. When the query fails, you are not in Limpet, and SGR 30-37 in
`CON:` select pens anyway.

![SGR 31 in Limpet shows pen 1, while 38;5;196 and 38;2 show red and orange](screenshots/sdk-limpet-compare.png)

*Line 2 sends `ESC [31m`. Limpet shows pen 1 of the dark theme, as `CON:`
would, not red.*

**Status.** Verified 2026-10-05 with `colors` and the comparison probe
(`tools/con_compare.c`). apkg uses this rule in its coloured output.

---

## Text styles

| Style | Sequence | Reported by the query |
| --- | --- | --- |
| Bold on, all off | SGR 1, SGR 0 | yes, `XTY_STYLE_BOLD_RESET` |
| Bold off | SGR 22 | no |
| Underline on/off | SGR 4 / 24 | no |
| Inverse on/off | SGR 7 / 27 | no |

Italic, blink, strike-through, double and curly underline are not
supported and are ignored.

**Detect.** Only bold and reset are reported. Underline and inverse have no
detection; a program that needs them must treat them as Limpet 0.1.0
behaviour, not a promise of the query.

**Fallback.** Do not carry meaning in a style alone.

**Status.** Bold and reset: verified. Underline and inverse:
released, host-tested (`tests/test_screen_model.c`), not checked on AROS
for this catalogue.

---

## Rewriting a line

**What it gives you.** Progress bars and status lines that update in place:
CR (byte 0x0D) returns to column 1, `CSI K` (or `CSI 0 K`) erases to the
end of the line.

**Detect.** Presentation query: `XTY_LINE_CR` and `XTY_LINE_ERASE_EOL`, both
needed. `limpet_can_rewrite_line()` checks both. CR alone would leave the
tail of a longer previous line on screen.

**Use.** `"\r" + text + "\033[K"`, flushed after each update. Keep the line
shorter than the window: ask the size before redrawing.

**Fallback.** A new line per step (10%, 20%, ...), never CR alone. Never
write CR updates to a file.

**Example.** [`progress`](../sdk/examples/progress.c).

![A progress bar at 47%, drawn in one line](screenshots/sdk-progress.png)

**Status.** Verified 2026-10-05 (`progress`, including Ctrl-C in the
middle of the bar, return code 5). `CON:` on the same system also honours
CR and `CSI K`, but does not answer the query, so a program cannot know
that without guessing.

---

## Window size

**What it gives you.** Rows and columns of the text grid.

**Detect and use.**

- **Presentation query** (preferred): `limpet_columns()` and
  `limpet_rows()`, 0 when unknown. No input involved.
- **`CSI 18 t`** in raw mode: Limpet answers `ESC [ 8 ; rows ; cols t` on
  the input stream. Needed only by programs that already parse input
  replies.

**The size is a snapshot.** There is no resize notification: no signal, no
event, no message in the input stream. Ask again before laying out text,
and after any long pause. Do not ask on every character: each query is a
round trip to the terminal's process.

**Limits.** At most 80 rows and 160 columns in Limpet 0.1.0.

**Fallback.** Assume 80 columns, wrap long lines yourself, or let the
terminal wrap them.

**Example.** [`geometry`](../sdk/examples/geometry.c).

![The geometry example reports 117x37, then 91x28 after a resize, then 117x37](screenshots/sdk-geometry.png)

**Status.** Query: verified, including a resize during the run (2026-10-05).
`CSI 18 t`: released, host-tested (`tests/test_geometry_reply.c`), checked
in a mainline development build; not run on ABIv11.

---

## Cursor movement and erasing

**Supported.** CUP (`CSI row ; col H` / `f`), CUU/CUD/CUF/CUB
(`CSI n A/B/C/D`), CNL/CPL (`E`/`F`), CHA (`G`, `` ` ``), VPA (`d`),
ED (`CSI J`, 0/1/2), EL (`CSI K`, 0/1/2), IL/DL (`CSI L`/`M`),
SU/SD (`CSI S`/`T`), scrolling margins (`CSI top ; bottom r`), reverse
index (`ESC M`), cursor visibility (`CSI ? 25 h/l`).

**Not supported.** Save/restore cursor (`ESC 7`/`8`, `CSI s`/`u`), origin
mode, setting tab stops, insert/delete/erase character (`CSI @`, `P`,
`X`), any device attribute (DA) reply.

**Detect.** None. Use these only after the presentation query succeeded.
`CON:` implements a different, Amiga console set.

**Status.** Released, host-tested (`tests/test_screen_model.c`,
`tests/test_cursor_queries.c`, `tests/test_status_area.c`). Native
full-screen programs using positioning and the alternate screen passed the
0.1.0 release regression on ABIv11.

---

## Cursor position report

`CSI 6 n` answers `ESC [ row ; col R`; `CSI ? 6 n` answers
`ESC [ ? row ; col R`; `CSI 5 n` answers `ESC [ 0 n`. Replies come on the
input stream, **in raw mode only**; in cooked mode Limpet sends no reply,
so that nothing appears at the Shell prompt.

A program that sends a query must read the reply with a time limit
(`WaitForChar()`), parse it among ordinary key presses, and never send a
query while the user is typing something else.

**Status.** Released, host-tested (`tests/test_cursor_queries.c`); checked
on a mainline development build. The raw-mode reply channel itself was
exercised on ABIv11 by the Kitty query (2026-10-05); the `CSI 6 n` reply
was not run there.

---

## Alternate screen

`CSI ? 1049 h` switches to a separate screen without scrollback,
`CSI ? 1049 l` returns to the primary screen with its text and cursor
restored.

**Detect.** None. A full-screen program must restore the primary screen on
every exit path, including Ctrl-C.

**Status.** Verified in the 0.1.0 release regression (native full-screen
program, resize, return to the Shell). Host test: `tests/test_alternate.c`.

---

## Unicode text

**What works.** UTF-8 output; CJK and other wide characters take two cells;
combining marks and grapheme clusters stay in one cell group; emoji are
drawn in monochrome (Noto Emoji). Width follows the bundled Unicode 17
tables, so a program that measures text with the same tables lines up.

**What does not.** Bidirectional text, colour emoji, and grapheme clusters
beyond the bounded length.

**Detect.** None. A successful presentation query means Limpet, which
decodes UTF-8. In `CON:` the same bytes show as Latin-1 characters.

![UTF-8 in CON: shows mojibake](screenshots/sdk-con-compare.png)

*The comparison probe in `CON:` on ABIv11 2026.09: line 1 is UTF-8, shown
as Latin-1; line 5 is an OSC 8 link, printed as text.*

**Status.** Verified (0.1.0 release regression, and the comparison probe
on 2026-10-05). Host tests: `tests/test_graphemes.c`,
`tests/test_width_table.py`.

---

## Hyperlinks

**What it gives you.** Text that carries a link target. In Limpet the user
hovers to see the target in the title bar, Ctrl+clicks to open it through
OpenURL, and Ctrl+Shift+clicks to copy it.

**Use.** OSC 8: `ESC ] 8 ; ; URI ESC \` label `ESC ] 8 ; ; ESC \`. The SDK's
`limpet_osc8_open()` refuses URIs with spaces, control characters or
non-ASCII bytes, so outside text cannot inject sequences.

**Limits.** Limpet opens only `http://` and `https://` targets of at most
2047 bytes. Other schemes are shown as text and do nothing on click. 64
explicit targets are remembered per window; older ones become plain text.

**Plain URLs.** `http://` and `https://` addresses in ordinary output are
detected without any program change, also across soft line wraps.

**Fallback.** Print the URL after the label. Never send OSC 8 to `CON:`: it
prints the sequence as text (see the screenshot above, line 5).

**Example.** [`hyperlink`](../sdk/examples/hyperlink.c).

![Title bar while hovering a link: Link: https://github.com/tomaszstaniak/limpet](screenshots/sdk-link-title.png)

**Status.** Display, underline on hover and the target in the title:
verified 2026-10-05. Opening through OpenURL: not verified on ABIv11 (it
needs an installed browser and OpenURL setup). Copying: verified on
mainline during development. Details: [hyperlinks.md](hyperlinks.md).

---

## Inline images

**What it gives you.** Pictures between lines of text, sent through the
terminal stream with a subset of the
[Kitty graphics protocol](https://sw.kovidgoyal.net/kitty/graphics-protocol/).

**Supported.** Raw RGB (`f=24`) and RGBA (`f=32`, blended over the text);
direct base64 transmission in chunks; transmit, display, delete and query;
image ids; `C=0`/`C=1`; reply control `q=0/1/2`. Full table:
[kitty-graphics.md](kitty-graphics.md).

**Not supported.** PNG, compression, files, shared memory, animation,
placement ids, scaling and cropping keys, Unicode placeholders.

**Limits.** 1024x1024 pixels and 4 MiB per image, 16 images, 16 MiB in
total.

**Detect.** Two steps:

1. The presentation query must succeed. Never send image data otherwise:
   `CON:` prints it as text.
2. Send `a=q` in raw mode and read the reply with a time limit:
   `limpet_kitty_query()` and `limpet_kitty_parse_reply()`.

**Things to know before drawing.**

- Limpet 0.1.0 does not scroll the screen to make room. An image placed
  near the bottom of the window is cut off, and `C=0` moves the cursor no
  further than the last row. Print blank lines first, move back up, draw
  with `C=1`, then move down. The [`image`](../sdk/examples/image.c)
  example shows how.
- The cell size in pixels is not reported. Reserve rows for a cell height
  smaller than the real one; the cost is a blank line or two.
- In cooked mode no reply is delivered. Use `q=2` unless you read replies.
- Delete images you no longer need (`limpet_kitty_delete(id, 1, ...)`);
  they count against the 16-image limit.
- Images are hidden while the user scrolls back and follow text only
  approximately after a width change.

![A 160x96 image drawn below a line of text](screenshots/sdk-image.png)

**Status.** Verified 2026-10-05 (`image`: query reply in raw mode,
transmission, deletion). Host tests: `tests/test_kitty_graphics.c` and the
Kitty part of `sdk/tests/test_sdk.c`.

---

## Keyboard input

**Cooked mode** (the default). Limpet edits the line itself (see line
editing in the [user-interface table](#user-interface-features)). The
program receives the finished line on Enter.

**Raw mode.** `SetMode(Input(), 1)`. The program receives every key as it
is pressed. Cursor keys arrive as `ESC [ A` to `ESC [ D`. In `CON:` they
arrive as the 8-bit CSI byte 0x9B followed by the letter. A program that
supports both consoles must decode both forms. Return to cooked mode with
`SetMode(Input(), 0)` on every exit path.

`WaitForChar()` and `Read()` behave as in `CON:`.

**Example.** [`keys`](../sdk/examples/keys.c), which runs unchanged in both
consoles.

**Status.** Verified 2026-10-05 in Limpet and in `CON:` (arrows, a letter,
Esc, Ctrl-C, cooked mode afterwards).

---

## Ctrl-C

- **Cooked mode:** Ctrl-C sends the break signal (`SIGBREAKF_CTRL_C`) to
  the Shell and the command it runs, as in `CON:`. Programs check it with
  `SetSignal()` or `CheckSignal()`.
- **Raw mode:** Ctrl-C arrives as byte 3. No signal is sent.

**Limits.** The break does not reach jobs started with `Run`. On a
multi-processor system one press can be skipped; press again. A program
that never checks for the signal cannot be stopped this way, as in `CON:`.

**Status.** Verified (release acceptance, and `progress` on 2026-10-05:
break at 33%, return code 5).

---

## Mouse reporting

`CSI ? 1000 h` (buttons), `CSI ? 1002 h` (motion while a button is held),
`CSI ? 1006 h` (SGR encoding). Reports are generated in raw mode only.
Shift+drag still selects text. Any-motion (1003), focus events and pixel
coordinates are not supported.

**Status.** Released, host-tested (`tests/test_mouse.c`,
`tests/test_mouse_release.c`); used by a Ratatui program on a mainline
development build. Not checked on ABIv11.

---

## User-interface features

These help the person at the keyboard. Programs cannot detect or control
them, and should not need to.

| Feature | In 0.1.0 | Status |
| --- | --- | --- |
| Scrollback | 256 lines at full width (160 columns), more for narrower windows, e.g. 512 at 80 columns | Verified |
| Reflow | text and scrollback rewrap when the width changes | Verified |
| Selection and clipboard | drag to select, RAmiga+C / RAmiga+V, system clipboard | Verified |
| Themes | dark (built in), light, high contrast | Verified |
| Line editing | command history, Tab completion of paths and commands | Released, host-tested (`tests/test_line_editor.c`, `tests/test_completion.c`) |
| End of input | F5 ends the Shell's input; F4 starts a new Shell | Verified |

Source of these results: the Limpet 0.1.0 release regression and
acceptance on AROS ABIv11 2026.09 and AROS One 1.3.

---

## Terminal settings (termios) and PTY

The source tree contains a pseudo-terminal, a DOS packet protocol for
terminal settings (control operations 1 to 12, cancellation and immediate
I/O) and an experimental termios layer for POSIX programs. They run only in
a **mainline** preview build that is not published, together with an
experimental `posixc.library`.

A Limpet 0.1.0 Shell session answers every one of those operations with
`ERROR_ACTION_NOT_KNOWN`. Do not build on them yet. What exists is
described in [the experimental guide](sdk/experimental.md).

**Status.** Experimental.

---

## SSH

An SSH client preview exists for mainline AROS (`examples/ssh/`). It is not
part of the ABIv11 release and has not been built for ABIv11.

**Status.** Experimental.
