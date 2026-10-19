# Limpet capabilities

This page describes **Limpet 0.6.0** and **SDK 0.4.0** for AROS ABIv11,
x86_64. It covers terminal features, application interfaces and the separate
experimental `LIMPET:` console handler.

Features added after 0.6.0 are listed under [Unreleased changes](#unreleased-changes).
For installation and platform requirements, see [compatibility](compatibility.md).

## Feature detection

The [presentation query](presentation.md) (operation 13) reports colours,
styles, line controls, window dimensions, colour interpretation and supported
input modes. It uses a DOS packet on the output handle without reading input.

Stream queries, including `CSI 6 n` and Kitty `a=q`, return replies through
input in raw mode. Applications must parse these replies alongside keyboard
input and use a timeout when waiting for them.

Unicode, scrollback and the alternate screen have no capability query.
A successful presentation query confirms only the fields and bits in its reply.
`XTY_CAPS` (operation 1) describes experimental TTY endpoints; it is not
supported by a released Shell session. See [control operations](protocols.md#control-operations).

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
| [Shift+Enter](#shiftenter) | `CSI > 4 ; 1 m`; presentation input bit | 0.6.0 |
| [Window title](#window-title) | OSC 0/2; presentation feature bit | 0.3.0 |
| [Command history search](#history-search) | Ctrl-R | 0.3.0 |
| [File drop](#file-drop) | drag Workbench icons onto the command line | 0.4.0 |
| [Read cancellation](#read-cancellation) | XTY_ACTION_CANCEL | 0.4.1 |
| [Find in output](#find-in-output) | RAmiga+F or Ctrl+Shift+F | 0.5.0 |
| [Settings](#settings) | ENV:Limpet/Limpet.conf | 0.5.0 |
| [Console handler](#limpet-console-handler) | LIMPET:; separate experimental archive | 0.5.0 |
| [Amiga console reports](#amiga-console-reports) | `CSI 0 SPACE q`, `CSI 6 n` with the 8-bit CSI | 0.6.0; `SPACE q` 8-bit only since 0.6.1 |
| [Ctrl-D/E/F, Ctrl-\\](#ctrl-c) | break signals and end of input, as in `CON:` | 0.6.0 |

## Presentation query

The presentation query reports the output handle's rendering capabilities.
Use the reply to select formatting; use plain text when the query is unsupported
or output is redirected.

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

### Query behaviour

Pass the output handle you will write to, normally `Output()`. Using `Input()`
would give the wrong result when output is redirected to a file.

- A clear bit in `known` means the corresponding field is unknown. A set bit
  with a zero value means the feature is unsupported. The SDK clears unknown fields.
- Ignore unknown bits and reserved fields (`values[9..11]`) so the client remains
  compatible with future protocol extensions.
- Dimensions apply at the time of the query. Query again after resizing or
  before updating a layout.
- The call waits for the handler's reply and has no timeout.
- For non-interactive output, the SDK returns `LIMPET_NOT_INTERACTIVE` without
  sending a packet.
- If the handler returns `ERROR_ACTION_NOT_KNOWN`, use plain-text output.

**Example:** [`hello`](../sdk/examples/hello.c).

![Output of the hello example in Limpet](screenshots/sdk-hello.png)

*`hello` (SDK 0.2.0) in Limpet 0.2.0 on ABIv11 2026.09: 24-bit colour,
pens, bold, line rewriting, 117x37 cells and the three input modes.*

## Colours

The presentation query reports one of three colour levels.

| Level | Value | Sequences (7-bit `ESC [`, semicolon form) |
| --- | --- | --- |
| ANSI16 | 1 | SGR 30-37, 90-97, 39 (foreground); 40-47, 100-107, 49 (background) |
| INDEXED256 | 2 | adds `38;5;n` and `48;5;n` |
| RGB | 3 | adds `38;2;r;g;b` and `48;2;r;g;b` |

Limpet 0.6.0 reports level 3. The colour capability covers semicolon forms
and 7-bit CSI only; it does not guarantee colon forms (`38:2::r:g:b`) or
the 8-bit CSI byte 0x9B.

**Detection:** Presentation query: known bit `XTY_PRESENT_KNOWN_COLORS`, value
in `color_level`. Use `limpet_color_level()`, which treats a level newer
than the SDK as unknown.

**Usage:** `limpet_sgr_ansi()` for the 16 colours, `limpet_sgr_rgb()` for
24-bit colour with automatic reduction to the 256-colour cube. End colours
with SGR 39/49 or SGR 0.

If colour is unavailable, omit SGR sequences and use text labels for status
and errors. Basic SGR colours follow the [Workbench pens rule](#sgr-30-37-as-workbench-pens).

**Example:** [`colors`](../sdk/examples/colors.c).

![The colors example in Limpet: 16 colours with samples and swatches, a 256-colour ramp and a 24-bit gradient](screenshots/sdk-colors.png)

*`colors` in Limpet, dark theme. Colour 0 is black text on a dark
background, which is why the example prints names in the default colour.*

## SGR 30-37 as Workbench pens

In native console sessions, SGR 30-37 and 40-47 select **Amiga screen pens
0-7**, matching `CON:`. For example, `ESC[31m` selects pen 1 rather than
ANSI red. This preserves the colours of the Shell prompt and native programs.

**Detection:** Presentation query: `XTY_COLOR_PENS` in `interpretation`,
valid only when the known bit `XTY_PRESENT_KNOWN_INTERPRETATION` is set.
`limpet_uses_pens()` checks both.

**Usage:** When pens are on, send the 16 ANSI colours as `38;5;0` to
`38;5;15` (`48;5;n` for background). These keep their ANSI meaning and are
guaranteed from colour level ANSI16 up. `limpet_sgr_ansi()` selects the appropriate sequence.

**Reset:** SGR 39 restores the theme's default foreground and drops a
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

Use text labels as well as styling for errors and status.

## Rewriting a line

CR (byte 0x0D) returns to column 1; `CSI K` (or `CSI 0 K`) erases to the
end of the line. Together they support progress bars and status lines updated
in place.

**Detection:** Presentation query: `XTY_LINE_CR` and `XTY_LINE_ERASE_EOL`, both
needed. `limpet_can_rewrite_line()` checks both. CR alone would leave the
tail of a longer previous line on screen.

**Usage:** `"\r" + text + "\033[K"`, flushed after each update. Keep the line
shorter than the window: ask the size before redrawing.

If line rewriting is unavailable or output is redirected, print each update
on a new line.

**Example:** [`progress`](../sdk/examples/progress.c).

![A progress bar at 47%, drawn in one line](screenshots/sdk-progress.png)

## Window size

The grid dimensions are available through:

- **Presentation query** (preferred): `limpet_columns()` and
  `limpet_rows()`, 0 when unknown. No input involved.
- **`CSI 18 t`** in raw mode: Limpet answers `ESC [ 8 ; rows ; cols t` on
  the input stream. Needed only by programs that already parse input
  replies.

Since **0.5.0**, POSIX programs can also obtain the current dimensions with
`ioctl(TIOCGWINSZ)`, including after resize. In 0.4.1 and earlier this path
could crash in `crt.library`; applications using it should require 0.5.0.
This fix does not provide a complete POSIX termios or PTY implementation.

There is no resize signal. Raw-mode applications can enable
[resize reports](#resize-reports); otherwise query dimensions before laying
out text. Each query requires a round trip to the terminal process.

**Limits:** At most 80 rows and 160 columns.

When dimensions are unavailable, use an 80-column layout or rely on terminal
line wrapping.

**Example:** [`geometry`](../sdk/examples/geometry.c).

![The geometry example reports 117x37, then 91x28 after a resize, then 117x37](screenshots/sdk-geometry.png)

## Cursor movement and erasing

**Supported:** CUP (`CSI row ; col H` / `f`), CUU/CUD/CUF/CUB
(`CSI n A/B/C/D`), CNL/CPL (`E`/`F`), CHA (`G`, `` ` ``), VPA (`d`),
ED (`CSI J`, 0/1/2), EL (`CSI K`, 0/1/2), IL/DL (`CSI L`/`M`),
SU/SD (`CSI S`/`T`), scrolling margins (`CSI top ; bottom r`), reverse
index (`ESC M`), cursor visibility (`CSI ? 25 h/l`).

**Unsupported:** Save/restore cursor (`ESC 7`/`8`, `CSI s`/`u`), origin
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

## Amiga console reports

Since **0.6.0**, a Shell session also answers the two reports of the Amiga
console, so a program written for `CON:` can ask Limpet for the window size
and the cursor position.

- `CSI 0 SPACE q` (or `CSI SPACE q`) sent with the 8-bit CSI (`0x9B`)
  answers `0x9B 1;1;rows;columns SPACE r`. Sent with `ESC [`, the same
  bytes are xterm's cursor-style request (DECSCUSR), which full-screen
  programs send when they quit; Limpet ignores it and sends nothing (0.6.0
  answered it, and the reply reached the Shell as input).
- `CSI 6 n` sent with the 8-bit CSI (`0x9B`) answers `0x9B row;column R`.
  Sent with `ESC [`, it answers `ESC [ row ; column R` as before.

Both are one-based, as AmigaOS answers. The `CON:` of ABIv11 answers one
lower. Replies come in raw mode only, like the other queries, and give the
new size after a resize.

## Alternate screen

`CSI ? 1049 h` switches to a separate screen without scrollback,
`CSI ? 1049 l` returns to the primary screen with its text and cursor
restored.

A full-screen program must restore the primary screen on
every exit path, including Ctrl-C.

## Unicode text

Limpet decodes UTF-8. CJK and other wide characters occupy two cells;
combining marks remain attached to their grapheme cluster. Emoji use the
monochrome Noto Emoji font. Cell widths follow the bundled Unicode 17 tables.

Bidirectional layout and colour emoji are unsupported. Cluster storage has
a fixed length limit.

**Detection:** There is no Unicode capability bit. Limpet decodes UTF-8;
encoding in other consoles depends on their configuration.

### CON: and Limpet comparison

The same output probe in the system console and in Limpet:

| CON: | Limpet |
| --- | --- |
| ![CON: shows UTF-8 as Latin-1 and prints the OSC 8 sequence as text](screenshots/sdk-con-compare.png) | ![Limpet renders Unicode, indexed and RGB colours, and an OSC 8 link](screenshots/sdk-limpet-compare.png) |

The probe prints UTF-8 text, a basic SGR pen colour, indexed colour, RGB
colour, an OSC 8 link and a line rewritten with CR and CSI K. In this AROS
ABIv11 2026.09 console, UTF-8 appears as Latin-1, indexed and RGB colour
are ignored, and OSC 8 is printed as text. Both consoles interpret basic
SGR colours as pens and support the line rewrite.

## Hyperlinks

OSC 8 attaches a URL to a text label. Hovering shows the target in the title
bar; Ctrl+click opens it through OpenURL, and Ctrl+Shift+click copies it.

**Usage:** OSC 8: `ESC ] 8 ; ; URI ESC \` label `ESC ] 8 ; ; ESC \`. The SDK's
`limpet_osc8_open()` refuses URIs with spaces, control characters or
non-ASCII bytes, so outside text cannot inject sequences.

**Limits:** Limpet opens only `http://` and `https://` targets of at most
2047 bytes. Other schemes are shown as text and do nothing on click. 64
explicit targets are remembered per window; older ones become plain text.

**Plain URLs:** `http://` and `https://` addresses in ordinary output are
detected without any program change, also across soft line wraps.

**Fallback:** Print the URL after the label. Never send OSC 8 to `CON:`: it
prints the sequence as text (see the screenshot above, line 5).

While the pointer is over a link the title shows `Link: <target> - Limpet`;
the previous title returns when the pointer leaves.

**Example:** [`hyperlink`](../sdk/examples/hyperlink.c).

![Title bar while hovering a link: Link: https://github.com/tomaszstaniak/limpet](screenshots/sdk-link-title.png)

## Window title

The title bar shows the current command or an application-provided title:

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

**Command name:** The Shell runs each command in its own
process and, while it runs, sets the CLI's module and its task name to the
command. Limpet reads that from the process reading the console, under the
same protection it uses for Ctrl-C. Not identified: jobs started with `Run`,
processes a command starts, and what a program is doing.

**Application title:** `ESC ] 2 ; text ESC \` (or BEL instead of `ESC \`;
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

Send a title when the application state changes. Limpet limits updates to
about four per second, displaying the latest title.

**Limits:** The title bar uses Latin-1: characters outside it show as `?`.
No title stack (`CSI 22 t` / `CSI 23 t`), no icon name (OSC 1), no title
query. A title sent by a `Run` job ends when the next command starts.

**Example:** [`title`](../sdk/examples/title.c).

## Inline images

Limpet displays inline RGB/RGBA images using a subset of the
[Kitty graphics protocol](https://sw.kovidgoyal.net/kitty/graphics-protocol/).

**Supported:** Raw RGB (`f=24`) and RGBA (`f=32`, blended over the text);
direct base64 transmission in chunks; transmit, display, delete and query;
image ids; `C=0`/`C=1`; reply control `q=0/1/2`. Full table:
[kitty-graphics.md](kitty-graphics.md).

**Unsupported:** JPEG/PNG decoding, compression, file or shared-memory
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

**Placement and cleanup:**

- Since 0.3.0, `C=0` (the default) scrolls text up when an image extends
  below the window. The cursor ends on the image's last row, one column
  past its right edge.
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

**Cooked mode** (default): Limpet edits the line itself (see line
editing in the [user-interface table](#user-interface-features)). The
program receives the finished line on Enter.

Characters arrive as UTF-8. A keymap gives the letters above 127 in the
system's 8-bit character set; since **0.6.1** Limpet converts them with the
set chosen in Locale preferences (`ENV:CHARSET`, for example ISO-8859-2 for
a Polish keymap), through codesets.library. Without `ENV:CHARSET`, or
without that library, they are read as Latin-1, as in earlier versions.

**Raw mode:** `SetMode(Input(), 1)`. The program receives every key as it
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

Since **0.6.0**, Ctrl-D, Ctrl-E and Ctrl-F send `SIGBREAKF_CTRL_D`, `_E`
and `_F` to the program reading the window in cooked mode, as in `CON:`;
Ctrl-D stops a running script. In raw mode they arrive as bytes 4, 5 and 6.

Ctrl-\ ends input once, as in `CON:`: text already typed reaches the
program without a line end, the next read returns 0, and later reads wait
for input again. At the Shell prompt the Shell ends. Unlike `CON:`, Ctrl-\
waits its turn behind keys and pastes typed before it. F5 still ends the
window's input for good.

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

**Input ordering:**

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

Mode 2048 sends window dimensions through the input stream when the
window is resized.

**Usage:** In raw mode, send `CSI ? 2048 h`. The program receives
`CSI 48 ; rows ; columns ; height ; width t` (text area height and width in
pixels) at once, and again after every size change. If the size changes
while a report is half delivered, one more report with the newest size
follows. No report is ever cut by other input or put inside a bracketed
paste. `CSI ? 2048 l` turns it off.

**Detection:** `XTY_INPUT_RESIZE_REPORT` in `LimpetPresentation.input` (known
bit 32). With Limpet 0.1.0, use the presentation query or `CSI 18 t`.

## Wheel reports

Mode 2100 reports wheel events without forwarding button clicks or drags.
Applications can scroll their content while the terminal retains text selection.
With mouse tracking modes 1000/1002, Shift+drag is required for terminal selection.

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

## Shift+Enter

A program with its own multi-line editor, such as a chat prompt or a REPL,
can use Enter to send and Shift+Enter to start a new line. Without this mode
both keys arrive as CR.

**Usage:** In raw mode, send `CSI > 4 ; 1 m`. Shift+Enter (Return or the
keypad Enter, with Shift alone) then arrives as `CSI 27 ; 2 ; 13 ~`. Enter,
and Enter with Ctrl, Alt or an Amiga key, still arrive as CR. `CSI > 4 m`,
`CSI > 4 ; 0 m`, `CSI > 4 n` and `CSI > m` turn the mode off.

This is the sequence xterm's modifyOtherKeys sends for Shift+Enter, so a
program that already reads it from xterm reads it here. Limpet implements
only this key: other keys are sent as before at any level, and the bit does
not mean that other modified keys are reported.

**Detection:** `XTY_INPUT_MODIFIED_ENTER` in `LimpetPresentation.input`
(known bit 32). Without it, offer another key for a new line: Ctrl-J
arrives as LF in every Limpet version and in `CON:`.

## Input modes end with raw mode

When a Limpet Shell window returns to cooked mode (a program calls
`SetMode(Input(), 0)` or ends), Limpet turns off mouse tracking 1000, 1002
and 1006, bracketed paste 2004, resize reports 2048, wheel reports 2100
and the Shift+Enter mode. This prevents modes from carrying over into the
Shell or the next program after the session returns to cooked mode.

Enable the required modes after each switch to raw mode.

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

### History search

At the Shell prompt, Ctrl-R searches the commands typed in this window
(32 by default, configurable from 1 to 500 in 0.5.0), not the output:

- type to narrow the search; Backspace removes a character;
- Ctrl-R again: the next older, distinct matching command;
- Enter puts the command on the line for editing; it does not run it;
- Esc restores the previous line and cursor position;
- arrows, Home, End, Delete and Tab accept the result and perform their usual action.

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

A refused drop leaves the command line unchanged and displays the reason
in the title bar. Drop is unavailable when:

- a program in the window reads keys itself (an editor, hardcode's full
  screen);
- a command is running, or typing or a paste is still going in;
- a history search is open;
- the About window is open;
- an object is not there (an icon without its file);
- a name is not UTF-8 text or holds a control character (Limpet sends
  commands as UTF-8);
- the line would exceed 1023 bytes, or there are more than 64 objects.

The AROS Shell miscounts quotes in names containing `"`. With such a name on the line, a later `;`
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

The separate **0.6.0 experimental handler** lets a program open a Limpet
console through DOS:

```text
NewShell WINDOW LIMPET:
```

It is distributed as `Limpet-handler-experimental-0.6.0-abiv11.zip`, not
installed by the main archive or apkg. Installation lasts until reboot and
does not replace `CON:`. Programs opening `CON:` still get the system console.

The released handler supports console names with geometry and title, cooked
and raw input, Shell scripts, redirection and multiple writers, and, since
0.6.0, the Amiga console's reports, Ctrl-D/E/F and Ctrl-\. Important
limits in **the 0.6.0 handler**:

- only one reader or `WaitForChar` request can be pending;
- `AUTO` opens the window immediately; `WINDOW` is rejected;
- raw-mode LF moves down without returning to column 1;
- the console information exposes dimensions, but is not a working
  `console.device` endpoint for direct device I/O;
- startup is slow and each window uses about 32.5 MiB. Memory recovery over
  repeated opens and closes has not been established.

For installation, see [the handler guide](limpet-handler.md#install-and-remove).
The guide includes development changes; the list above describes the
released 0.6.0 archive.

## Terminal settings (termios) and PTY

The source tree contains a pseudo-terminal, a DOS packet protocol for
terminal settings (control operations 1 to 12, cancellation and immediate
I/O) and an experimental termios layer for POSIX programs. They run only in
a **mainline** preview build that is not published, together with an
experimental `posixc.library`. The released `LIMPET:` handler is a different
interface and does not supply that POSIX runtime.

A Limpet Shell session answers every one of those operations with
`ERROR_ACTION_NOT_KNOWN`, except that from Limpet 0.4.1 it takes back a client's
own pending read with `XTY_ACTION_CANCEL`. See [the experimental guide](sdk/experimental.md)
for the preview APIs.

## SSH

An SSH client preview exists for mainline AROS (`examples/ssh/`). It is not
part of the ABIv11 release and has not been built for ABIv11.

## Unreleased changes

None that affect programs. The development tree may hold work on the
experimental handler that is not in the 0.6.0 archive; see
[the handler guide](limpet-handler.md).

## References

- [Protocol reference](protocols.md) and [SDK API](sdk/api.md)
- [Changes by release](changes.md)
- [0.6.0 verification record](verification-0.6.0.md), with earlier records
  linked from the corresponding releases
