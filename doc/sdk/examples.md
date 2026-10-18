# Examples

Eight programs in `sdk/examples/` (in the archive: `examples/`). Each is
one C file that uses only the public SDK, `dos.library` and the C library.

**Target:** AROS ABIv11 x86_64. **Build:** `make CC=... AROS_SDK=...
ABI=abiv11 examples`, results in `build/abiv11/`; see
[getting started](getting-started.md).

| Program | Shows | In `CON:` | Ends |
| --- | --- | --- | --- |
| `hello` | the presentation query and its answer, including the input modes | says the console was not identified | at once |
| `colors` | 16, 256 and 24-bit colour, pens | names only, no sequences | at once |
| `progress` | a line rewritten in place, width from the window | one line per 10% | after 10 s, or Ctrl-C |
| `geometry` | the window size as a snapshot | says the size is unknown | after 20 s, or Ctrl-C |
| `hyperlink` | OSC 8 links with a fallback | label and URL as text | at once |
| `image` | the Kitty subset: query, draw, delete | sends nothing | key, or 30 s |
| `keys` | raw-mode key input in both consoles | works, cursor keys as 0x9B | `q`, Ctrl-C, or 60 s idle |
| `title` (0.3.0) | the window title following a program's state | the states as text only | after 8 s, or Ctrl-C |

## hello

Asks once and prints the answer field by field, saying "unknown" where the
terminal said nothing. Return code 0 in every console; 5 (WARN) only for a
malformed reply or a DOS error.

## colors

The 16 ANSI colours as a text sample and a background swatch, then a
256-colour ramp and a 24-bit gradient, each only at the reported level. The
colour name is printed in the default colour, because colour 0 is invisible
as text on a dark theme. Ends every colour with SGR 39/49.

## progress

100 simulated steps of 0.1 s. In Limpet: one line, `CR` + text + `CSI K`,
green filled part, width taken from the window at the start and every 25
steps. Elsewhere: `0%`, `10%`, ... on separate lines. Ctrl-C prints
`***Break at N%` on a new line, with default colours, and returns 5.

Limit: a resize between two size checks makes one or more bars too wide or
too narrow until the next check.

## geometry

Prints the size, then asks twice a second for 20 seconds and prints each
change. Dimensions are polled; this example does not enable resize reports.

## title

Four states, two seconds each, shown as lines and, when the terminal
confirmed it, in the window title (`title demo: waiting for the server -
Limpet`). The title is sent only when the state changes. When the program
ends Limpet shows `Shell` again by itself. In `CON:`, an older Limpet or
with output to a file the program prints the states as text only.

## hyperlink

Two links and one invalid URI. In Limpet the labels are links; hover shows
the target in the title bar. The invalid one (it contains a space) is
printed as text in every console. Opening needs OpenURL and a browser on
the AROS system.

## image

1. Presentation query; stops without sending anything if it fails.
2. Raw mode, Kitty `a=q`, then cooked mode again. The whole reply must
   arrive within one second of writing the query; a missing reply ends the exchange after that deadline.
3. Prints blank lines for the image, moves back up, draws a 160x96 RGB
   picture with the cursor kept in place, moves below it. Limpet 0.3.0
   would make room by itself for `C=0`; the example reserves rows so that
   it works the same in 0.1.0 to 0.2.1.
4. Raw mode, waits for a key (30 s at most), cooked mode, deletes the image
   and frees its pixels.

Every `SetMode`, write and read result is checked. Whatever fails after
raw mode was entered, the console is switched back to cooked mode, and a
drawn or partly sent image is deleted. Return code 0 on success or when
graphics are unavailable, 5 when the console misbehaved, 20 when writing
the image failed.

Limit: the reserved space assumes a cell at least 12 pixels high. Limpet's
default font is taller, so one or two blank lines remain under the image.

## keys

Raw mode until `q`, Ctrl-C or 60 seconds without a key. Names cursor keys,
Esc, control bytes and printable characters. Shows both forms of cursor
keys: `ESC [ A` from Limpet and `0x9B A` from `CON:`. Uses the SDK only to
decide on colour.

Limit: function keys and other longer sequences are reported as "other key
sequence". A lone Esc is recognised by 50 ms of silence after it.

Build and runtime records: [SDK 0.3.0](verification-0.3.0.md) and
[SDK 0.2.0](verification-0.2.0.md).
