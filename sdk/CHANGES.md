# Limpet SDK changes

## 0.2.0 (2026-10-07)

The first SDK release. Works with Limpet 0.1.0 and 0.2.0.

Added:

- `LimpetPresentation.input`, appended to the struct, valid with known bit
  32 (`XTY_PRESENT_KNOWN_INPUT`): the input modes a raw-mode program may
  turn on in Limpet 0.2.0.
  - `XTY_INPUT_BRACKETED_PASTE` (1): DEC mode 2004.
  - `XTY_INPUT_RESIZE_REPORT` (2): DEC mode 2048.
  - `XTY_INPUT_WHEEL_REPORT` (4): Limpet's private mode 2100, wheel steps
    as SGR mouse reports while buttons stay with the terminal.
  Limpet 0.1.0 leaves the bit and the field clear.
- `hello` prints the input modes.

Changed:

- Binary interface: `LimpetPresentation` is 4 bytes longer. Rebuild every
  object with this SDK. `limpet_query_presentation` and
  `limpet_presentation_decode` have new link names
  (`..._v2`), so an object built with the 0.1 header fails to link
  instead of overflowing its struct. Source code is unchanged.

Fixed:

- `limpet_kitty_transmit`: after a failed write past the first chunk, an
  empty final chunk is sent, so Limpet drops the partial image at once
  instead of keeping its buffer until the next command.
- `examples/image.c`: one deadline for the whole Kitty reply (it allowed a
  second per byte); `SetMode`, `Read` and `Write` results checked; cooked
  mode restored and the image deleted on every error path.
- `examples/keys.c`: `SetMode` results checked.

Protocol header `tty_control.h`: SHA-256
`a12f68daf4e92cbb265aaef39668d7787824f2b7363f94e039701b5a4166eab4`.

## 0.1.0 (2026-10-05, not published)

Prepared but never released; superseded by 0.2.0. Its sources are kept in
`sdk/tests/compat-0.1.0` for the compatibility test.

- `limpet_query_presentation()`: ask the console behind an output handle
  for its colour level, styles, line rewriting, size and colour
  interpretation (control operation 13). Checks `IsInteractive()` first;
  maps `ERROR_ACTION_NOT_KNOWN` to `LIMPET_UNSUPPORTED`.
- `limpet_control()`: the validated transport for control blocks.
- `limpet_presentation_request()`, `limpet_presentation_decode()`: build a
  request and check a reply without DOS, for tests and custom transports.
- Decisions: `limpet_color_level()`, `limpet_uses_pens()`,
  `limpet_has_bold()`, `limpet_can_rewrite_line()`, `limpet_columns()`,
  `limpet_rows()`.
- Builders: `limpet_sgr_ansi()` (follows the pens rule),
  `limpet_sgr_rgb()` (24-bit, or the 256-colour cube),
  `limpet_osc8_open()` (rejects URIs that could inject sequences).
- Kitty graphics subset: `limpet_kitty_transmit()`, `limpet_kitty_delete()`,
  `limpet_kitty_query()`, `limpet_kitty_parse_reply()`, `limpet_write_dos()`.
- Seven examples.
- Protocol header `tty_control.h` identical to the one in Limpet 0.1.0
  (SHA-256 `921bc72d2e4c94ff937e22eacdf51d0da88995c30b7e56b9006e829fbb72cdf1`).
