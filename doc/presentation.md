# Presentation query (operation 13)

A program that writes to Limpet's console can ask what the terminal will
render before it decorates its output: colours, bold, line controls,
current size and how the basic colour codes are read. The query is a DOS
packet on the program's own output handle; on any other console it fails
with `ERROR_ACTION_NOT_KNOWN`, and the program should write plain text.
Constants and the wire layout are in [`src/tty_control.h`](../src/tty_control.h);
a minimal client is [`tools/presentation_probe.c`](../tools/presentation_probe.c).

## Request and reply

`XTY_GET_PRESENTATION = 13`, in an `XtyControl` block of version 1.

Request: version 1, operation 13, reserved words and all values zero.
Reply uses the existing validated control envelope:

| Field | Meaning |
| --- | --- |
| values[0] | Known-field mask: bit 0 colors, bit 1 styles, bit 2 line editing, bit 3 geometry, bit 4 color interpretation |
| values[1] | Color level: 0 none, 1 ANSI 16, 2 indexed 256, 3 RGB truecolor |
| values[2] | Style mask: bit 0 SGR bold plus SGR reset |
| values[3] | Line mask: bit 0 CR, bit 1 CSI K erase from cursor to end of line |
| values[4] | Current rows |
| values[5] | Current columns |
| values[6] | Color interpretation mask: bit 0 `XTY_COLOR_PENS` |
| values[7..11] | Reserved, zero |

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

- `XTY_COLOR_PENS` counts only when known bit 4 is set. PENS without the
  known bit is no claim; with the interpretation unknown the client keeps
  its ANSI behaviour.
- With PENS known, `38;5;0..15` and `48;5;0..15` are guaranteed from level
  `ANSI16` (values[1] = 1) up, with their ANSI palette meaning.
- `SGR 39` restores the theme's default foreground and drops a foreground
  pen. It does not bring back an earlier pen and changes neither the
  background nor bold. `SGR 49` does the same for the background.
- The next Shell prompt sets its own pens with its own sequences; a client
  need not restore them, only end its own colours (`39`/`49` or `0`).

Public enums and masks are in `src/tty_control.h`: `XTY_PRESENT_KNOWN_*`,
`XTY_COLOR_*` (including `XTY_COLOR_PENS`), `XTY_STYLE_BOLD_RESET`,
`XTY_LINE_CR` and `XTY_LINE_ERASE_EOL`.
The header specifies 7-bit ESC [ sequences and the exact supported SGR codes.
The query is sent directly; XTY_CAPS is not a prerequisite on native sessions.
Unsupported operations return the normal unsupported-action error without
modifying the caller block. Unknown future bits must be ignored by clients.
