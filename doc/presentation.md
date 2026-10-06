# Presentation query (operation 13)

A program that writes to Limpet's console can ask what the terminal will
render before it decorates its output: colours, bold, line controls,
current size and how the basic colour codes are read. The query is a DOS
packet on the program's own output handle; on any other console it fails
with `ERROR_ACTION_NOT_KNOWN`, and the program should write plain text.
Constants and the wire layout are in
[`sdk/include/limpet/tty_control.h`](../sdk/include/limpet/tty_control.h),
the header both the terminal and the [SDK](sdk/api.md) are built from.
`limpet_query_presentation()` does everything on this page for you; the
[`hello`](../sdk/examples/hello.c) example shows it in use.

## Request and reply

`XTY_GET_PRESENTATION = 13`, in an `XtyControl` block of version 1.

Request: version 1, operation 13, reserved words and all values zero.
The reply uses the control envelope described in [protocols.md](protocols.md#envelope):

| Field | Meaning |
| --- | --- |
| values[0] | Known-field mask: bit 0 colours, bit 1 styles, bit 2 line editing, bit 3 geometry, bit 4 colour interpretation |
| values[1] | Colour level: 0 none, 1 ANSI 16, 2 indexed 256, 3 RGB 24-bit |
| values[2] | Style mask: bit 0 SGR bold plus SGR reset |
| values[3] | Line mask: bit 0 CR, bit 1 CSI K erase from cursor to end of line |
| values[4] | Current rows |
| values[5] | Current columns |
| values[6] | Colour interpretation mask: bit 0 `XTY_COLOR_PENS` |
| values[7] | Input features (known bit 32, after 0.1.0): bit 0 bracketed paste (DEC 2004), bit 1 resize reports (DEC 2048) |
| values[8..11] | Reserved, zero |

A clear known bit means unknown, not unsupported. A set known bit with a
zero feature value means confirmed absent. Geometry is known only when both
dimensions are positive. The geometry comes from the live grid after resize
processing, not a startup default. It is a snapshot, not a resize subscription.

**Pens.** On an AROS console session the renderer reads SGR
30-37 and 40-47 as Amiga screen pens 0-7, as the system console does, so
the Shell's own prompt shows as in `CON:`. It says so with
`XTY_COLOR_PENS` in values[6]. A client that sees the bit must send ANSI
colours as `38;5;n` / `48;5;n` (n 0-15), which keep their palette meaning;
the basic SGR codes then do not mean ANSI colours.

Exact rules, as in the header:

- `XTY_COLOR_PENS` counts only when known bit 4 is set; without it the bit
  means nothing, and with the interpretation unknown the client keeps
  its ANSI behaviour.
- With PENS known, `38;5;0..15` and `48;5;0..15` are guaranteed from level
  `ANSI16` (values[1] = 1) up, with their ANSI palette meaning.
- `SGR 39` restores the theme's default foreground and drops a foreground
  pen. It does not bring back an earlier pen and changes neither the
  background nor bold. `SGR 49` does the same for the background.
- The next Shell prompt sets its own pens with its own sequences; a client
  need not restore them, only end its own colours (`39`/`49` or `0`).

Public enums and masks are in `limpet/tty_control.h`: `XTY_PRESENT_KNOWN_*`,
`XTY_COLOR_*` (including `XTY_COLOR_PENS`), `XTY_STYLE_BOLD_RESET`,
`XTY_LINE_CR` and `XTY_LINE_ERASE_EOL`.
The header specifies 7-bit ESC [ sequences and the exact supported SGR codes.
Send the query directly; `XTY_CAPS` is not needed first. A console that
does not support it returns an error and leaves your block unchanged.
Ignore bits you do not know.
