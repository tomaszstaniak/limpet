# What to use, and when

Most programs need very little from Limpet. Start at the top of this list
and stop at the first entry that covers what you want to do.

| You want to | Use | SDK needed |
| --- | --- | --- |
| print text and read lines | standard C or DOS I/O | no |
| add colour, bold, a progress line | the presentation query, then SGR and CR | yes, or the protocol by hand |
| lay out text to the window width | the presentation query, asked again before each layout | yes |
| print links | OSC 8, after the query | yes, for the query |
| show pictures | Kitty graphics subset, after two checks | yes |
| read single keys | `SetMode()` raw mode, `WaitForChar()`, `Read()` | no |
| a full-screen interface | raw mode plus cursor sequences and the alternate screen | for detection only |
| run a POSIX/termios program | not available in a public release yet | experimental |

## Plain command-line tools

If your program prints results and reads lines, it already works in
Limpet. Unicode output (UTF-8) is shown correctly, long output goes into
scrollback, and the window rewraps it when resized. Do not add the SDK just
because Limpet exists. A program that prints `RAM:` paths and file sizes
gains nothing from it.

Keep doing the things that work everywhere: write to `Output()`, check
`SIGBREAKF_CTRL_C` in long loops, return the usual `RETURN_*` codes.

## Colour and progress

Use the [presentation query](../capabilities.md#presentation-query). It
answers without touching the keyboard, and it fails cleanly in `CON:` and
for files. Then:

- colours through `limpet_sgr_ansi()` / `limpet_sgr_rgb()`, which handle the
  [pens rule](../capabilities.md#sgr-30-37-as-workbench-pens);
- a progress line only when `limpet_can_rewrite_line()` is true; otherwise
  print a new line per step;
- the bar width from `limpet_columns()`, asked again every second or so.

Give users a way to turn decoration off, for example a `PLAIN` switch.
Never decide from the window title, the program name, an environment
variable or `Input()`.

Example: [`colors`](../../sdk/examples/colors.c),
[`progress`](../../sdk/examples/progress.c). Real use: apkg, the AROS
package manager, decides colour, bold, table width and its download bar
this way.

## Native AROS programs that want more

The SDK API is the whole public interface in this release: one query and
sequence builders. It does not wrap Intuition, it does not open windows,
and it does not replace `dos.library`. Link `libLimpet.a`, or compile its
two source files into your program.

## Links

Send OSC 8 only after the presentation query succeeded, and print the
target after the label otherwise. See
[`hyperlink`](../../sdk/examples/hyperlink.c).

## Images

Two checks, in order: the presentation query (no image bytes to a console
that fails it), then the Kitty `a=q` query in raw mode with a time limit.
Make room before drawing, because Limpet 0.1.0 does not scroll for the
image. See [`image`](../../sdk/examples/image.c) and
[inline images](../capabilities.md#inline-images).

## Single keys and full-screen interfaces

Raw mode is plain `dos.library` and works in `CON:` too. The differences a
program must handle:

- cursor keys: `ESC [ A` in Limpet, `0x9B A` in `CON:`;
- Ctrl-C in raw mode is byte 3 in both, not a signal;
- full-screen sequences (`CSI ? 1049 h`, cursor positioning) are the VT
  set in Limpet and the Amiga console set in `CON:`.

Restore cooked mode and the primary screen on every exit path. See
[`keys`](../../sdk/examples/keys.c).

There is no resize event. A full-screen program asks for the size again
(query or `CSI 18 t`) after each key press or on a timer, and redraws when
it changed.

## Portable POSIX and TUI programs

Programs written for termios (Ratatui, crossterm, ncurses) need a
pseudo-terminal and a termios layer. Both exist only in an unpublished
mainline preview with an experimental system library. Nothing in Limpet
0.1.0 for ABIv11 supports them. Read
[the experimental guide](experimental.md) if you want to follow that work.

## Reading answers from the input stream

Some protocols answer on the input stream: `CSI 6 n`, `CSI 18 t`, the Kitty
query. In Limpet 0.1.0 these answers are sent only in raw mode. A program
that uses them must:

- switch to raw mode for the exchange and back afterwards;
- read with a time limit (`WaitForChar()`), because a console that does
  not understand the query never answers;
- expect key presses mixed in with the answer, and keep them;
- not send queries while the user is typing.

When the presentation query gives you the same information, use it
instead: it has none of these problems.
