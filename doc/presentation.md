# Presentation query (operation 13)

Operation 13 queries rendering capabilities through a DOS packet on the
application's output handle. Unsupported consoles return
`ERROR_ACTION_NOT_KNOWN`; applications should then use plain text.
Constants and the wire layout are in
[`sdk/include/limpet/tty_control.h`](../sdk/include/limpet/tty_control.h),
the header both the terminal and the [SDK](sdk/api.md) are built from.
`limpet_query_presentation()` builds, sends and decodes the request; the
[`hello`](../sdk/examples/hello.c) example shows it in use.

## Request and reply

`XTY_GET_PRESENTATION = 13`, in an `XtyControl` block of version 1.

Request: version 1, operation 13, reserved words and all values zero.
The reply uses the control envelope described in [protocols.md](protocols.md#envelope):

| Field | Meaning |
| --- | --- |
| values[0] | Known-field mask: bit 0 colours, bit 1 styles, bit 2 line editing, bit 3 geometry, bit 4 colour interpretation, bit 5 input modes (0.2.0), bit 6 features (0.3.0) |
| values[1] | Colour level: 0 none, 1 ANSI 16, 2 indexed 256, 3 RGB 24-bit |
| values[2] | Style mask: bit 0 SGR bold plus SGR reset |
| values[3] | Line mask: bit 0 CR, bit 1 CSI K erase from cursor to end of line |
| values[4] | Current rows |
| values[5] | Current columns |
| values[6] | Colour interpretation mask: bit 0 `XTY_COLOR_PENS` |
| values[7] | Input modes a raw-mode client may turn on, valid with known bit 5 (value 32): bit 0 bracketed paste (DEC 2004), bit 1 resize reports (DEC 2048), bit 2 wheel reports (private 2100). Limpet 0.2.0 and later; 0.1.0 leaves the field and its known bit clear |
| values[8] | Terminal features, valid with known bit 6 (value 64, `XTY_PRESENT_KNOWN_FEATURES`): bit 0 `XTY_FEATURE_TITLE`, OSC 0/2 set the window title. Limpet 0.3.0 and later; 0.2.x leaves the field and its known bit clear, so an answered query alone never means titles work |
| values[9..11] | Reserved, zero |

A clear known bit means unknown, not unsupported. A set known bit with a
zero feature value means confirmed absent. Geometry is known only when both
dimensions are positive. The geometry comes from the live grid after resize
processing, not a startup default. Applications must query again for updated dimensions or enable resize reports.

**Pens.** On an AROS console session the renderer reads SGR
30-37 and 40-47 as Amiga screen pens 0-7, as the system console does, so
the Shell's own prompt shows as in `CON:`. This is reported by
`XTY_COLOR_PENS` in values[6]. A client that sees the bit must send ANSI
colours as `38;5;n` / `48;5;n` (n 0-15), which keep their palette meaning;
the basic SGR codes then do not mean ANSI colours.

Colour interpretation rules:

- `XTY_COLOR_PENS` is valid only when known bit 4 is set. If colour
  interpretation is unknown, retain standard ANSI behaviour.
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
Ignore unknown bits and reserved fields.
