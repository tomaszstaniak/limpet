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
| values[0] | Known-field mask: bit 0 colours, bit 1 styles, bit 2 line editing, bit 3 geometry, bit 4 colour interpretation, bit 5 input modes (0.2.0), bit 6 features (0.3.0), bit 7 text (0.8.0) |
| values[1] | Colour level: 0 none, 1 ANSI 16, 2 indexed 256, 3 RGB 24-bit |
| values[2] | Style mask: bit 0 SGR bold plus SGR reset |
| values[3] | Line mask: bit 0 CR, bit 1 CSI K erase from cursor to end of line |
| values[4] | Current rows |
| values[5] | Current columns |
| values[6] | Colour interpretation mask: bit 0 `XTY_COLOR_PENS` |
| values[7] | Input modes a raw-mode client may turn on, valid with known bit 5 (value 32): bit 0 bracketed paste (DEC 2004), bit 1 resize reports (DEC 2048), bit 2 wheel reports (private 2100), bit 3 Shift+Enter as `CSI 27 ; 2 ; 13 ~` after `CSI > 4 ; 1 m` (Limpet 0.6.0). Limpet 0.2.0 and later; 0.1.0 leaves the field and its known bit clear |
| values[8] | Terminal features, valid with known bit 6 (value 64, `XTY_PRESENT_KNOWN_FEATURES`): bit 0 `XTY_FEATURE_TITLE`, OSC 0/2 set the window title. Limpet 0.3.0 and later; 0.2.x leaves the field and its known bit clear, so an answered query alone never means titles work |
| values[9] | How text is shown, valid with known bit 7 (value 128, `XTY_PRESENT_KNOWN_TEXT`): bit 0 `XTY_TEXT_UTF8`, written bytes are decoded as UTF-8 (an ill-formed sequence shows as U+FFFD); bit 1 `XTY_TEXT_WIDE`, characters of East Asian width W or F and emoji presentation sequences take two cells; bit 2 `XTY_TEXT_COMBINING`, combining marks, variation selectors and ZWJ sequences join the cell before them. Limpet 0.8.0 sets all three. Without the known bit nothing is known about the encoding: write ASCII |
| values[10..11] | Reserved, zero |

## Terminal identity (operation 14)

`XTY_GET_IDENTITY = 14`, same envelope and request rules as operation 13.
It names the terminal for display, for example in a system information
program; it is not a capability test.

| Field | Meaning |
| --- | --- |
| values[0] | Known mask: bit 0 name, bit 1 version |
| values[1..3] | Version major, minor, patch, valid with bit 1 |
| values[4..11] | Name: 1 to 31 bytes of printable ASCII (0x20..0x7e), then NUL bytes up to 32, as stored in memory (not as 32-bit numbers), valid with bit 0 |

Limpet 0.8.0 answers name `Limpet` and its version, in a Shell session and
on the experimental PTY. `CON:` and earlier Limpet versions answer
`ERROR_ACTION_NOT_KNOWN`.

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

## Optional styles, links and icon fonts (2026-10-21)

Presentation retains its 64-byte packet and 36-byte SDK structure. Styles are
individual enable/reset bit pairs: bold 1, dim 2, italic 4, underline 8, blink 16,
inverse 32, hidden 64, strike 128. This renderer reports bold/underline/inverse.
Unknown style bits never authorize an SGR code. Features add OSC8=2,
NERD_ICONS_EZA=4 and ICON_WIDTH_ONE=8 alongside TITLE=1. OSC8 uses the existing
streaming BEL/ST parser, including fragmented writes. Icon bits describe all 268
scalars in eza v0.23.5 (98442ab17c2c3738701b62a7e060b1431ae2d6ea), recorded in
`src/eza_icon_contract.h`; extensions require a new repertoire capability.

The attached renderer supplies optional feature evidence at query time, cached
only for its current font generation. Every mapped glyph must exist in the actual
font chain, rasterize successfully, advance one cell, fit horizontally and have
terminal scalar width 1. Generic/headless PTYs have no evidence callback and
never gain icon bits. Removing the font or selecting a deficient font suppresses
icons; command-line `always` cannot create font support.

Host checks: `make test-presentation test-icon-contract test-sdk`; the public SDK
compatibility suite covers old clients and current packet replies. On macOS arm64
with Homebrew FreeType, the unmodified bundled Nerd Font at 13px passed all268
lookups, grid8x17, all advances512/64px, zero clipped bitmaps. FontTools4.67.0
confirmed cmap coverage268/268 and common advance1233/2048em. These host results
and the ABIv11 compile do not establish native rendering or user interaction.
Native acceptance must query the actual Limpet output, inspect icons across long
and grid listings, confirm underline/inverse and click an OSC8 link, and verify
files/pipes remain plain when eza requests `always`.

### Local file targets

OSC8 registration accepts `file:///Volume:path` as a local AmigaDOS target.
The decoder preserves UTF8/filename bytes from percent triplets and rejects
remote authorities, relative or Unix paths, malformed escapes, NUL and controls.
HTTP(S) links keep the existing OpenURL path. File links bypass OpenURL command
templates and pass the decoded path directly to `OpenWorkbenchObjectA` inside
the existing link worker. This requests Workbench's normal
file/default-tool or drawer handling; availability and success remain native
runtime results, not properties of the parser capability.

`make abiv11-file-link-probe` builds `build/abiv11/probes/test_file_link_aros`.
Run it with stdout redirected to a named report. It creates and retains only
`RAM:eza link fixture.txt`, feeds its OSC8 sequence one byte at a time, resolves
the linked cell to `file:///RAM:eza%20link%20fixture.txt`, decodes it and checks
`Lock`/`NameFromLock` against the real fixture. RC0 establishes actual native
path lookup. Then list that fixture with eza's confirmed hyperlink mode in
Limpet and use the ordinary link gesture to check Workbench dispatch. Preserve
the result before deleting that exact RAM fixture. Parser tests alone establish
neither Workbench availability nor a successful file open.
