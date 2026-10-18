# Limpet capabilities

This page describes **Limpet 0.5.0** and **SDK 0.3.0** for AROS ABIv11,
x86_64. It covers terminal features, application interfaces and the separate
experimental `LIMPET:` console handler.

Features added after 0.5.0 are listed under [Unreleased changes](#unreleased-changes).
For installation and platform requirements, see [compatibility](compatibility.md).

## How a program finds out

Use the interface that reports the feature you need:

1. **The presentation query** (operation 13). A DOS packet on your output
   handle. It reports colours, bold, line rewriting, the window size, how
   SGR 30-37 are read and, from 0.2.0, which input modes a raw-mode
   program may turn on. It does not read input. This is the method to use
   first. [Details](presentation.md).
2. **Stream queries** such as `CSI 6 n` or the Kitty `a=q` query. The
   answer arrives as input, and Limpet sends it only while the console is
   in raw mode. Use them only after the presentation query has succeeded.
3. **No query.** Some features (Unicode, scrollback, the alternate screen)
   have no query. A successful presentation query confirms only the fields and bits
   in its reply. It does not advertise every feature listed here.

`XTY_CAPS` (operation 1) is **not** a way to find any of this: a Limpet
Shell session does not answer it. See
[the protocol reference](protocols.md#control-operations).

## Summary

| Feature | Interface or shortcut | Since |
| --- | --- | --- |
| [Colours](#colours), [styles](#text-styles), [line updates](#rewriting-a-line) | SGR, CR, CSI K; presentation query | 0.1.0 |
| [Window size](#window-size) | presentation query, CSI 18 t; `TIOCGWINSZ` fixed in 0.5.0 | 0.1.0 |
| [Cursor movement](#cursor-movement-and-erasing), [alternate screen](#alternate-screen) | VT sequences | 0.1.0 |
| [Unicode](#unicode-text) | UTF-8 text | 0.1.0 |
| [Links](#hyperlinks) | OSC 8; automatic HTTP/HTTPS detection | 0.1.0 |
| [Inline images](#inline-images) | Kitty graphics subset | 0.1.0 |
| [Mouse reporting](#mouse-reporting) | modes 1000, 1002, 1006 | 0.1.0 |
| [Bracketed paste](#bracketed-paste) | mode 2004; presentation input bit | 0.2.0 |
| [Resize reports](#resize-reports) | mode 2048; presentation input bit | 0.2.0 |
| [Wheel reports](#wheel-reports) | Limpet mode 2100; presentation input bit | 0.2.0 |
| [Window title](#window-title) | OSC 0/2; presentation feature bit | 0.3.0 |
| [Command history search](#history-search) | Ctrl-R | 0.3.0 |
| [File drop](#file-drop) | drag Workbench icons onto the command line | 0.4.0 |
| [Read cancellation](#read-cancellation) | XTY_ACTION_CANCEL | 0.4.1 |
| [Find in output](#find-in-output) | RAmiga+F or Ctrl+Shift+F | 0.5.0 |
| [Settings](#settings) | ENV:Limpet/Limpet.conf | 0.5.0 |
| [Console handler](#limpet-console-handler) | LIMPET:; separate experimental archive | 0.5.0 |

## Presentation query

The query reports colour level, bold and
reset, line rewriting, current size, how SGR 30-37 are read and, from
0.2.0, the input modes a raw-mode program may turn on.

On AROS the same program can write to `CON:`, to a
file, to a pipe or to Limpet. Escape sequences that look good in Limpet
print as garbage in `CON:` and end up in files. Use the reply to choose supported formatting and keep redirected output plain.

**Detection:** Send the query directly. `ERROR_ACTION_NOT_KNOWN` means the
console does not implement it.

**Usage:**

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

**Reply handling.**

- **Query the handle you write to.** Usually `Output()`. Never `Input()`:
  with `program >file` the input is still the terminal but the text goes to
  the file.
- **A clear known bit means unknown, not absent.** A set bit with a zero
  value means confirmed absent. The SDK zeroes fields whose bit is clear.
- **Ignore what you do not know.** Unknown bits and the reserved values
  9 to 11 must not make a client fall back to plain text. A newer Limpet
  may set them. (Limpet 0.2.0 started using `values[7]` this way: a 0.1.0
  client ignores it and still decodes everything else.)
- **The answer is a snapshot.** It describes the console at that moment.
  The size in particular changes when the user resizes the window.
- **It blocks** until the handler replies. Limpet and `CON:` reply at once.
  There is no timeout: DOS packets have none.
- **Non-interactive handles get no packet.** The SDK checks
  `IsInteractive()` first and returns `LIMPET_NOT_INTERACTIVE`.
- **`ERROR_ACTION_NOT_KNOWN`** comes from `CON:`, from consoles that are
  not Limpet, and from a Limpet session that is closing. All three mean
  "write plain text".

**Example:** [`hello`](../sdk/examples/hello.c).

![Output of the hello example in Limpet](screenshots/sdk-hello.png)

*`hello` (SDK 0.2.0) in Limpet 0.2.0 on ABIv11 2026.09: 24-bit colour,
pens, bold, line rewriting, 117x37 cells and the three input modes.*



## Colours

The presentation query reports one of three colour levels.

| Level | Value | Promised sequences (7-bit `ESC [` only, semicolon form) |
| --- | --- | --- |
| ANSI16 | 1 | SGR 30-37, 90-97, 39 (foreground); 40-47, 100-107, 49 (background) |
| INDEXED256 | 2 | adds `38;5;n` and `48;5;n` |
| RGB | 3 | adds `38;2;r;g;b` and `48;2;r;g;b` |

Limpet 0.5.0 reports level 3. Colon forms (`38:2::r:g:b`) are not
promised. The 8-bit CSI byte 0x9B is not promised.

**Detection:** Presentation query: known bit `XTY_PRESENT_KNOWN_COLORS`, value
in `color_level`. Use `limpet_color_level()`, which treats a level newer
than the SDK as unknown.

**Usage:** `limpet_sgr_ansi()` for the 16 colours, `limpet_sgr_rgb()` for
24-bit colour with automatic reduction to the 256-colour cube. End colours
with SGR 39/49 or SGR 0.

**Fallback:** No colour at all. Every coloured word must also make sense as
plain text: write "error:", not just red. Read the
[pens section](#sgr-30-37-as-workbench-pens) before writing SGR 30-37.

**Example:** [`colors`](../sdk/examples/colors.c).

![The colors example in Limpet: 16 colours with samples and swatches, a 256-colour ramp and a 24-bit gradient](screenshots/sdk-colors.png)

*`colors` in Limpet, dark theme. Colour 0 is black text on a dark
background, which is why the example prints names in the default colour.*



## SGR 30-37 as Workbench pens

The AROS Shell prompt and other native programs
look the same in Limpet as in `CON:`, because Limpet reads SGR 30-37 and
40-47 as **Amiga screen pens 0-7**, exactly like the system console. Pen 1
is not red; it is whatever the Workbench palette says.

Native programs written for `CON:` keep their
colours. The cost falls on programs that mean ANSI red when they write
`ESC [31m`.

**Detection:** Presentation query: `XTY_COLOR_PENS` in `interpretation`,
valid only when the known bit `XTY_PRESENT_KNOWN_INTERPRETATION` is set.
`limpet_uses_pens()` checks both.

**Usage:** When pens are on, send the 16 ANSI colours as `38;5;0` to
`38;5;15` (`48;5;n` for background). These keep their ANSI meaning and are
guaranteed from colour level ANSI16 up. `limpet_sgr_ansi()` does this
choice for you.

**Reset.** SGR 39 restores the theme's default foreground and drops a
foreground pen. It does not bring back an earlier pen, and it leaves
background and bold alone (SGR 49 likewise). The next Shell prompt sets its
own pens, so a program only has to end its own colours.

**Fallback:** When the interpretation is unknown, keep normal ANSI
behaviour. When the query fails, use plain text. SGR 30-37 in `CON:` select pens,
not ANSI palette colours.

![SGR 31 in Limpet shows pen 1, while 38;5;196 and 38;2 show red and orange](screenshots/sdk-limpet-compare.png)

*Line 2 sends `ESC [31m`. Limpet shows pen 1 of the dark theme, as `CON:`
would, not red.*



## Text styles

| Style | Sequence | Reported by the query |
| --- | --- | --- |
| Bold on, all off | SGR 1, SGR 0 | yes, `XTY_STYLE_BOLD_RESET` |
| Bold off | SGR 22 | no |
| Underline on/off | SGR 4 / 24 | no |
| Inverse on/off | SGR 7 / 27 | no |

Italic, blink, strike-through, double and curly underline are not
supported and are ignored.

**Detection:** Only bold and reset are reported. Underline and inverse have no
detection; a program that needs them must treat them as Limpet
behaviour, not a promise of the query.

**Fallback:** Do not carry meaning in a style alone.



## Rewriting a line

Progress bars and status lines that update in place:
CR (byte 0x0D) returns to column 1, `CSI K` (or `CSI 0 K`) erases to the
end of the line.

**Detection:** Presentation query: `XTY_LINE_CR` and `XTY_LINE_ERASE_EOL`, both
needed. `limpet_can_rewrite_line()` checks both. CR alone would leave the
tail of a longer previous line on screen.

**Usage:** `"\r" + text + "\033[K"`, flushed after each update. Keep the line
shorter than the window: ask the size before redrawing.

**Fallback:** A new line per step (10%, 20%, ...), never CR alone. Never
write CR updates to a file.

**Example:** [`progress`](../sdk/examples/progress.c).

![A progress bar at 47%, drawn in one line](screenshots/sdk-progress.png)



## Window size

Rows and columns of the text grid.

**Detect and use.**

- **Presentation query** (preferred): `limpet_columns()` and
  `limpet_rows()`, 0 when unknown. No input involved.
- **`CSI 18 t`** in raw mode: Limpet answers `ESC [ 8 ; rows ; cols t` on
  the input stream. Needed only by programs that already parse input
  replies.

Since **0.5.0**, POSIX programs can also obtain the current dimensions with
`ioctl(TIOCGWINSZ)`, including after resize. In 0.4.1 and earlier this path
could crash in `crt.library`; applications using it should require 0.5.0.
This fix does not provide a complete POSIX termios or PTY implementation.

**The size is a snapshot.** There is no resize signal. A raw-mode program
can ask for [resize reports](#resize-reports) in its input; otherwise ask
again before laying out text,
and after any long pause. Do not ask on every character: each query is a
round trip to the terminal's process.

**Limits:** At most 80 rows and 160 columns.

**Fallback:** Assume 80 columns, wrap long lines yourself, or let the
terminal wrap them.

**Example:** [`geometry`](../sdk/examples/geometry.c).

![The geometry example reports 117x37, then 91x28 after a resize, then 117x37](screenshots/sdk-geometry.png)



## Cursor movement and erasing

**Supported.** CUP (`CSI row ; col H` / `f`), CUU/CUD/CUF/CUB
(`CSI n A/B/C/D`), CNL/CPL (`E`/`F`), CHA (`G`, `` ` ``), VPA (`d`),
ED (`CSI J`, 0/1/2), EL (`CSI K`, 0/1/2), IL/DL (`CSI L`/`M`),
SU/SD (`CSI S`/`T`), scrolling margins (`CSI top ; bottom r`), reverse
index (`ESC M`), cursor visibility (`CSI ? 25 h/l`).

**Not supported.** Save/restore cursor (`ESC 7`/`8`, `CSI s`/`u`), origin
mode, setting tab stops, insert/delete/erase character (`CSI @`, `P`,
`X`), any device attribute (DA) reply.

**Detection:** These sequences have no individual capability bits.
`CON:` implements a different, Amiga console set.



## Cursor position report

`CSI 6 n` answers `ESC [ row ; col R`; `CSI ? 6 n` answers
`ESC [ ? row ; col R`; `CSI 5 n` answers `ESC [ 0 n`. Replies come on the
input stream, **in raw mode only**; in cooked mode Limpet sends no reply,
so that nothing appears at the Shell prompt.

A program that sends a query must read the reply with a time limit
(`WaitForChar()`), parse it among ordinary key presses, and never send a
query while the user is typing something else.



## Alternate screen

`CSI ? 1049 h` switches to a separate screen without scrollback,
`CSI ? 1049 l` returns to the primary screen with its text and cursor
restored.

A full-screen program must restore the primary screen on
every exit path, including Ctrl-C.



## Unicode text

**What works.** UTF-8 output; CJK and other wide characters take two cells;
combining marks and grapheme clusters stay in one cell group; emoji are
drawn in monochrome (Noto Emoji). Width follows the bundled Unicode 17
tables, so a program that measures text with the same tables lines up.

**What does not.** Bidirectional text, colour emoji, and grapheme clusters
beyond the bounded length.

**Detection:** There is no Unicode capability bit. Limpet decodes UTF-8;
encoding in other consoles depends on their configuration.

![UTF-8 in CON: shows mojibake](screenshots/sdk-con-compare.png)

*The comparison probe in `CON:` on ABIv11 2026.09: line 1 is UTF-8, shown
as Latin-1; line 5 is an OSC 8 link, printed as text.*



## Hyperlinks

Text that carries a link target. In Limpet the user
hovers to see the target in the title bar, Ctrl+clicks to open it through
OpenURL, and Ctrl+Shift+clicks to copy it.

**Usage:** OSC 8: `ESC ] 8 ; ; URI ESC \` label `ESC ] 8 ; ; ESC \`. The SDK's
`limpet_osc8_open()` refuses URIs with spaces, control characters or
non-ASCII bytes, so outside text cannot inject sequences.

**Limits:** Limpet opens only `http://` and `https://` targets of at most
2047 bytes. Other schemes are shown as text and do nothing on click. 64
explicit targets are remembered per window; older ones become plain text.

**Plain URLs.** `http://` and `https://` addresses in ordinary output are
detected without any program change, also across soft line wraps.

**Fallback:** Print the URL after the label. Never send OSC 8 to `CON:`: it
prints the sequence as text (see the screenshot above, line 5).

While the pointer is over a link the title shows `Link: <target> - Limpet`;
it goes back to the window title below when the pointer leaves.

**Example:** [`hyperlink`](../sdk/examples/hyperlink.c).

![Title bar while hovering a link: Link: https://github.com/tomaszstaniak/limpet](screenshots/sdk-link-title.png)



## Window title

The title bar says what runs in the window and,
when the program reports it, what that program is doing:

| Situation | Title |
| --- | --- |
| Shell waiting at its prompt | `Shell - Limpet` |
| a command runs | `Wait - Limpet` (the command's name, without its path) |
| the program set a title | `title demo: receiving - Limpet` |
| pointer over a link | `Link: https://... - Limpet` |
| the session ended | `Limpet` |

Terminal messages come after the name (`Shell - Limpet | Copied`), so a
narrow title bar cuts the message, not the name. Messages that report
something done disappear after about four seconds; those that ask for an
action (a multi-line paste waiting for F3) stay. Key hints are in the About
window, not in the title.

**What Limpet reads itself.** The Shell runs each command in its own
process and, while it runs, sets the CLI's module and its task name to the
command. Limpet reads that from the process reading the console, under the
same protection it uses for Ctrl-C. Not identified: jobs started with `Run`,
processes a command starts, and what a program is doing.

**What a program sends.** `ESC ] 2 ; text ESC \` (or BEL instead of `ESC \`;
OSC 0 works the same). The text must be 1 to 255 bytes of valid UTF-8 with
no control characters (C0, DEL, C1); otherwise the whole sequence is
ignored. An empty text drops the program's title. The title lasts while the
command that sent it runs: it goes when that command ends, another command
starts, the session ends, or a new title replaces it. A program does not
need to reset it on exit.

**Detection:** Send it only when the presentation reply has
`XTY_PRESENT_KNOWN_FEATURES` with `XTY_FEATURE_TITLE` (SDK:
`limpet_can_set_title()`). Limpet 0.2.x answers the query without that bit
and ignores the sequence; `CON:` prints it as text.

**Usage:**

```c
char seq[LIMPET_TITLE_MAX + 8];
size_t n = limpet_osc_title(seq, sizeof seq, &p, "Build: linking");
if (n) limpet_write_dos((void *)Output(), seq, n);
```

Send a title when your state changes, never with each line or token:
Limpet applies at most about four title changes a second and shows the
latest.

**Limits:** The title bar uses Latin-1: characters outside it show as `?`.
No title stack (`CSI 22 t` / `CSI 23 t`), no icon name (OSC 1), no title
query. A title sent by a `Run` job ends when the next command starts.

**Example:** [`title`](../sdk/examples/title.c).



## Inline images

Pictures between lines of text, sent through the
terminal stream with a subset of the
[Kitty graphics protocol](https://sw.kovidgoyal.net/kitty/graphics-protocol/).

**Supported.** Raw RGB (`f=24`) and RGBA (`f=32`, blended over the text);
direct base64 transmission in chunks; transmit, display, delete and query;
image ids; `C=0`/`C=1`; reply control `q=0/1/2`. Full table:
[kitty-graphics.md](kitty-graphics.md).

**Not supported.** JPEG/PNG decoding, compression, file or shared-memory
transmission, Kitty animation-frame commands, placement ids, scaling and
cropping keys, and Unicode placeholders. A program can still animate an
image by repeatedly transmitting replacement pixels under the same image id.

**Limits:** 1024x1024 pixels and 4 MiB per image, 16 images, 16 MiB in
total.

**Detection:** Two steps:

1. The presentation query must succeed. Never send image data otherwise:
   `CON:` prints it as text.
2. Send `a=q` in raw mode and read the reply with a time limit:
   `limpet_kitty_query()` and `limpet_kitty_parse_reply()`.

**Placement and cleanup.**

- With `C=0` (the default) Limpet 0.3.0 makes room: when the image would
  pass the bottom of the window the text scrolls up first, so the whole
  image shows, and the cursor ends on its last row, one column past it.
  Limpet 0.1.0 to 0.2.1 do not scroll: the image is cut off and `C=0`
  moves the cursor no further than the last row. For every version, print
  blank lines first, move back up, draw with `C=1`, then move down; the
  [`image`](../sdk/examples/image.c) example shows how. `C=1` never moves
  the cursor or scrolls.
- `ESC [ 2 J` (clear) removes the images on the screen shown, and
  entering the alternate screen removes that screen's images (0.3.0). The
  images stay transmitted: `a=p` shows one again.
- The cell size in pixels is not reported. Reserve rows for a cell height
  smaller than the real one; the cost is a blank line or two.
- In cooked mode no reply is delivered. Use `q=2` unless you read replies.
- Delete images you no longer need (`limpet_kitty_delete(id, 1, ...)`);
  they count against the 16-image limit.
- Images are hidden while the user scrolls back and follow text only
  approximately after a width change.

![A 160x96 image drawn below a line of text](screenshots/sdk-image.png)



## Keyboard input

**Cooked mode** (the default). Limpet edits the line itself (see line
editing in the [user-interface table](#user-interface-features)). The
program receives the finished line on Enter.

**Raw mode.** `SetMode(Input(), 1)`. The program receives every key as it
is pressed. Cursor keys arrive as `ESC [ A` to `ESC [ D`. In `CON:` they
arrive as the 8-bit CSI byte 0x9B followed by the letter. A program that
supports both consoles must decode both forms. Return to cooked mode with
`SetMode(Input(), 0)` on every exit path.

`Read()` returns input bytes or zero at EOF. `WaitForChar()` returning
false does not distinguish a timeout from EOF; do not use it alone to
detect that the console has closed. See [read cancellation](#read-cancellation)
for clients that keep an asynchronous read pending.

**Example:** [`keys`](../sdk/examples/keys.c), which runs unchanged in both
consoles.



## Ctrl-C

- **Cooked mode:** Ctrl-C sends the break signal (`SIGBREAKF_CTRL_C`) to
  the Shell and the command it runs, as in `CON:`. Programs check it with
  `SetSignal()` or `CheckSignal()`.
- **Raw mode:** Ctrl-C arrives as byte 3. No signal is sent.

After a break the Shell shows one new prompt (0.3.0; earlier versions
printed two on one line after interrupting a running command). Ctrl-C at
an empty prompt shows `^C` and a new prompt.

**Limits:** The break does not reach jobs started with `Run`. On a
multi-processor system one press can be skipped; press again. A program
that never checks for the signal cannot be stopped this way, as in `CON:`.



## Mouse reporting

`CSI ? 1000 h` (buttons), `CSI ? 1002 h` (motion while a button is held),
`CSI ? 1006 h` (SGR encoding). Reports are generated in raw mode only.
Shift+drag still selects text. Any-motion (1003), focus events and pixel
coordinates are not supported.


## Bracketed paste

Bracketed paste marks clipboard text as one block so a raw-mode program
can distinguish pasted newlines from commands entered at the keyboard.

**Usage:** After `SetMode(Input(), 1)`, send `CSI ? 2004 h`. A paste then
arrives as `CSI 200 ~`, the text, `CSI 201 ~`, and Limpet does not ask for
its multi-line confirmation. Send `CSI ? 2004 l` before leaving raw mode.

**Guarantees.**

- The text never contains ESC or C1 controls: Limpet refuses such a
  clipboard whole, so a paste cannot end its own frame.
- Nothing is inserted between the markers: key presses, mouse and wheel
  reports, query replies and resize reports wait until `CSI 201 ~` has
  been delivered, however slowly the program reads.
- A paste cancelled with Esc still ends with `CSI 201 ~`.
- A new paste waits until the previous one has been delivered completely.
- If the program leaves raw mode during a paste, the rest of it is
  dropped; it never reaches the Shell unconfirmed.

**Detection:** `XTY_INPUT_BRACKETED_PASTE` in `LimpetPresentation.input`, valid
with known bit 32 (`XTY_PRESENT_KNOWN_INPUT`). Limpet 0.1.0 reports
neither; it ignores `CSI ? 2004 h` and pastes unframed, with the
confirmation.


## Resize reports

A full-screen program learns about every size change
in its input, instead of asking on a timer.

**Usage:** In raw mode, send `CSI ? 2048 h`. The program receives
`CSI 48 ; rows ; columns ; height ; width t` (text area height and width in
pixels) at once, and again after every size change. If the size changes
while a report is half delivered, one more report with the newest size
follows. No report is ever cut by other input or put inside a bracketed
paste. `CSI ? 2048 l` turns it off.

**Detection:** `XTY_INPUT_RESIZE_REPORT` in `LimpetPresentation.input` (known
bit 32). With Limpet 0.1.0, use the presentation query or `CSI 18 t`.


## Wheel reports

A full-screen program can scroll its own content with
the mouse wheel while text selection stays with the terminal. Ordinary
mouse tracking (1000/1002) would take the buttons as well, and the user
could no longer select and copy.

**Usage:** In raw mode, send `CSI ? 2100 h`. Each wheel step arrives as an SGR
mouse report, `CSI < 64 ; column ; row M` for up and `CSI < 65 ; column ;
row M` for down, plus 16 for Ctrl and 8 for Alt. Buttons and drags are not
reported. Shift with the wheel still scrolls Limpet's own history. If
mouse tracking 1000 or 1002 is also on, it takes precedence and the wheel
is reported as part of it. `CSI ? 2100 l` turns it off.

Mode 2100 is a Limpet extension. Do not assume that another terminal
supports it; check the presentation bit before enabling it.

**Detection:** `XTY_INPUT_WHEEL_REPORT` in `LimpetPresentation.input` (known
bit 32). Never send `CSI ? 2100 h` without it.


## Input modes end with raw mode

When a Limpet Shell window returns to cooked mode (a program calls
`SetMode(Input(), 0)` or ends), Limpet turns off mouse tracking 1000, 1002
and 1006, bracketed paste 2004, resize reports 2048 and wheel reports
2100. A program that crashes or forgets to clean up therefore cannot leave
the Shell, or the next program, with modes it never asked for.

**Consequence for programs.** Set these modes after every switch to raw
mode, not once at start-up.



## User-interface features

| Feature | Behaviour |
| --- | --- |
| Scrollback | 256 lines at the maximum width by default; configurable from 100 to 1000 in 0.5.0. Narrower windows retain more rows. |
| Reflow | Text and history rewrap when the window width changes. |
| Selection and clipboard | Drag to select; RAmiga+C copies and RAmiga+V pastes through the system clipboard. |
| Selection during output | Unrelated updates leave the selection intact. Changes to selected text, scrolling or switching screens clear it. |
| Themes | Built-in dark theme, plus light and high-contrast theme files. |
| Line editing | Command history, Ctrl-R search and Tab completion of paths and commands. |
| End of input | F5 permanently ends the session's input; F4 starts a new Shell. |

Programs do not need a special API to use these features. Search, settings
and file drop are described below.

### History search

At the Shell prompt, Ctrl-R searches the commands typed in this window
(32 by default, configurable from 1 to 500 in 0.5.0), not the output:

- type to narrow the search; Backspace removes a character;
- Ctrl-R again: the next older command that matches and reads differently;
- Enter puts the command on the line for editing; it does not run it;
- Esc puts back what was on the line before, cursor included;
- arrows, Home, End, Delete and Tab take the command and then do their job.

The line shows `(reverse-i-search)` and the query; when nothing matches it
shows `(failed reverse-i-search)` and the line stays as it was. Matching
ignores case for ASCII letters. A program in raw mode receives Ctrl-R as
byte 0x12, as before.

### Find in output

Since 0.5.0, **RAmiga+F**, **Ctrl+Shift+F**, or **Edit > Find in output**
searches the current screen and scrollback.

- Type the text to find. Matching is literal and case-insensitive for ASCII
  letters; other characters must match exactly. There are no regular expressions
  or Unicode normalisation.
- Up moves to an older match; Down moves to a newer one.
- Enter closes Find and leaves the view at the result. Esc restores the
  previous view.
- RAmiga+V or Ctrl+Shift+V pastes the clipboard's first line into the query.
- Ctrl-C closes Find and interrupts the running command.

Incoming output does not move the view. Resizing keeps the same match where
possible; matches disappear when their text leaves the scrollback. Soft-wrapped
rows are searched as one line, but a match cannot cross a real line break.
While Find is open, typing goes to the search field, not the running program.

Find is unavailable on the alternate screen: a full-screen application's
private history is not stored by Limpet. Use that application's search.
See [search and settings](search-and-settings.md) for the full key list.

### Settings

Since 0.5.0, Limpet reads `ENV:Limpet/Limpet.conf` when a window opens,
falling back to `ENVARC:Limpet/Limpet.conf` if the ENV file is absent.
Use ENVARC for settings that survive a reboot and copy the file to ENV to
apply it during the current session.

| Key | Range or value | Default |
| --- | --- | --- |
| `font` | path to a monospace TrueType or OpenType font | `FONTS:TrueType/VeraMono.ttf` |
| `font_size` | 8–20 pixels | 13 |
| `theme` | path to a theme file | built-in dark theme |
| `scrollback` | 100–1000 lines at 160 columns | 256 |
| `history` | 1–500 commands | 32 |

**Project > Reload settings** applies the theme immediately. Font, size and
history limits apply to new windows. A bad file is rejected as a whole, with
the line number in the title bar; a failed reload preserves the current
settings. An unavailable font falls back to Vera Mono.

Scrollback uses about 2.7 MB per 100 lines, including the workspace needed
for reflow. Command history is kept only for the lifetime of the window.
The command-line `--theme` option overrides the file's theme.
See [the file format](search-and-settings.md#settings) for examples and limits.

### File drop

Drag one or more icons from a Wanderer drawer or the desktop onto a Limpet
window at the Shell prompt. The command line gets the full path of each
object at the cursor, written as Shell arguments. Nothing runs until you
press Enter.

- Paths are separated by spaces. A path with a space or a character the
  Shell reads specially is put in double quotes. Inside the quotes, `*"`,
  `**`, `*$` and `` *` `` stand for `"`, `*`, `$` and the backtick.
  Programs receive the path exactly, through `ReadArgs` and through C's
  `argv`.
- A drawer or a disk gives its own path (`"RAM Disk:docs"`, `Work:`). A
  file's icon gives the file, never its `.info`.
- The paths start with the volume name, as the system names the object:
  `"RAM Disk:..."` for a file in `RAM:`.
- A command that treats its argument as a pattern (`List`, `Copy`,
  `Delete`) still reads `#?()[]|~` in a name as pattern characters.

The line stays as it was, and the title bar says why, when the drop
cannot go in:

- a program in the window reads keys itself (an editor, hardcode's full
  screen);
- a command is running, or typing or a paste is still going in;
- a history search is open;
- the About window is open;
- an object is not there (an icon without its file);
- a name is not UTF-8 text or holds a control character (Limpet sends
  commands as UTF-8);
- the line would exceed 1023 bytes, or there are more than 64 objects.

One more refusal comes from the Shell itself. The AROS Shell miscounts
quotes when a name holds `"`. With such a name on the line, a later `;`
inside quotes would end the command early. Limpet refuses that drop. Drop
the two names one after the other with the `;` first, or separately.

`LimpetPTY` sessions (including SSH) and the experimental `LIMPET:` handler
do not take drops. Programs cannot detect or control the feature.


## Read cancellation

Since **0.4.1**, a Shell session accepts `XTY_ACTION_CANCEL` for a pending
read belonging to the same client. The request identifies the original DOS
packet, not a sequence number. The session replies to the read first with
`-1` / `ERROR_BREAK`, then to the cancel with `DOSTRUE`.

There is no capability bit: attempt cancellation and handle refusal. Limpet
0.4.0 and earlier, and `CON:`, return `ERROR_ACTION_NOT_KNOWN`. A read already
answered returns `ERROR_OBJECT_NOT_FOUND`. Neither failure cancels a read.

Keep both packets, their buffers and reply port alive until their replies
have been collected. A timeout is not permission to free a pending request.
See [the packet contract](sdk/experimental.md) and `limpet/tty_control.h`.
The broader PTY cancellation API remains experimental.

## LIMPET: console handler

The separate **0.5.0 experimental handler** lets a program open a Limpet
console through DOS:

```text
NewShell WINDOW LIMPET:
```

It is distributed as `Limpet-handler-experimental-0.5.0-abiv11.zip`, not
installed by the main archive or apkg. Installation lasts until reboot and
does not replace `CON:`. Programs opening `CON:` still get the system console.

The released handler supports console names with geometry and title, cooked
and raw input, Shell scripts, redirection and multiple writers. Important
limits in **the 0.5.0 handler**:

- only one reader or `WaitForChar` request can be pending;
- `AUTO` opens the window immediately; `WINDOW` is rejected;
- Amiga window-bounds reports, Ctrl-D/E/F signals and Ctrl-\ EOF are not
  available in that release;
- raw-mode LF moves down without returning to column 1;
- the console information exposes dimensions, but is not a working
  `console.device` endpoint for direct device I/O;
- startup is slow and each window uses about 32.5 MiB. Memory recovery over
  repeated opens and closes has not been established.

For installation, see [the handler guide](limpet-handler.md#install-and-remove).
That guide also tracks development; do not assume an unreleased feature is
in the 0.5.0 archive.

## Terminal settings (termios) and PTY

The source tree contains a pseudo-terminal, a DOS packet protocol for
terminal settings (control operations 1 to 12, cancellation and immediate
I/O) and an experimental termios layer for POSIX programs. They run only in
a **mainline** preview build that is not published, together with an
experimental `posixc.library`. The released `LIMPET:` handler is a different
interface and does not supply that POSIX runtime.

A Limpet Shell session answers every one of those operations with
`ERROR_ACTION_NOT_KNOWN`, except that from Limpet 0.4.1 it takes back a client's
own pending read with `XTY_ACTION_CANCEL`. For those preview APIs, see [the experimental guide](sdk/experimental.md).



## SSH

An SSH client preview exists for mainline AROS (`examples/ssh/`). It is not
part of the ABIv11 release and has not been built for ABIv11.


## Unreleased changes

The local development tree adds Amiga console window-bounds and cursor
reports, cooked-mode Ctrl-D/E/F signals, and Ctrl-\ to end input once.
These changes also affect ordinary Shell windows because the session code
is shared. They are **not part of Limpet 0.5.0 or its handler archive**.
See [the handler guide](limpet-handler.md) for current contracts and remaining
compatibility gaps. F5 remains the permanent end-of-input shortcut.

## References

- [Protocol reference](protocols.md) and [SDK API](sdk/api.md)
- [Changes by release](changes.md)
- [0.5.0 verification record](verification-0.5.0.md), with earlier records
  linked from the corresponding releases
