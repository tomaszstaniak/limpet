# Limpet SDK changes

## 0.5.0 (2026-10-24)

Works with Limpet 0.1.0 to 0.7.0.

Added:

- Style bits in `LimpetPresentation.styles` for each attribute and its reset:
  `XTY_STYLE_DIM_RESET`, `XTY_STYLE_ITALIC_RESET`,
  `XTY_STYLE_UNDERLINE_RESET`, `XTY_STYLE_BLINK_RESET`,
  `XTY_STYLE_INVERSE_RESET`, `XTY_STYLE_HIDDEN_RESET`,
  `XTY_STYLE_STRIKE_RESET`. Limpet 0.7.0 sets underline and inverse, the
  styles it draws.
- Feature bits in `LimpetPresentation.features`: `XTY_FEATURE_OSC8` (the
  terminal opens OSC 8 links), `XTY_FEATURE_NERD_ICONS_EZA` and
  `XTY_FEATURE_ICON_WIDTH_ONE` (its font has the file-type icons of eza
  0.23.5, each one cell wide; a program should use the icons only when both
  are set). Limpet 0.7.0 sets the icon bits only when its font has every one
  of those icons, one cell wide; its bundled Nerd Font does.

Unchanged: structures, packet sizes, operations and function names. Programs
built with SDK 0.3.0 or 0.4.0 work unchanged and ignore the new bits.

## 0.4.0 (2026-10-18)

Works with Limpet 0.1.0 to 0.6.0.

Added:

- Input bit `XTY_INPUT_MODIFIED_ENTER` (8) in `LimpetPresentation.input`:
  in raw mode, after `CSI > 4 ; 1 m`, Shift+Enter arrives as
  `CSI 27 ; 2 ; 13 ~` (Limpet 0.6.0). Limpet 0.5.0 and earlier leave the
  bit clear.
- `hello` lists the new input mode.

Changed:

- Nothing in the binary interface: `LimpetPresentation` and the link names
  are the same as in 0.3.0, so objects built with SDK 0.3.0 link with this
  library unchanged.
- `test_sdk_compat.sh` also checks an SDK 0.3.0 client (vendored in
  `sdk/tests/compat-0.3.0` from tag `sdk-v0.3.0`), and that an object built
  with the 0.3.0 header links with this library and decodes the reply.

Protocol header `tty_control.h`: SHA-256
`53c8c07797ad897a64ad54a60877f1ae9887536d86087c66a6eb4bff7c015907`.

## 0.3.0 (2026-10-11)

Works with Limpet 0.1.0 to 0.3.0.

Added:

- `LimpetPresentation.features`, appended to the struct, valid with known
  bit 64 (`XTY_PRESENT_KNOWN_FEATURES`). Bit `XTY_FEATURE_TITLE` (1): OSC 0
  and OSC 2 set the window title (Limpet 0.3.0). Limpet 0.2.x and earlier
  leave the bit and the field clear.
- `limpet_can_set_title()` and `limpet_osc_title()`, which builds
  `ESC ] 2 ; text ESC \` from 1..255 bytes of valid UTF-8 without control
  characters, only for a terminal that confirmed the feature;
  `LIMPET_TITLE_MAX`, `LIMPET_TITLE_RESET`.
- Example `title`; `hello` reports the feature.

Changed:

- Binary interface: `LimpetPresentation` is 4 bytes longer. Rebuild every
  object with this SDK. `limpet_query_presentation` and
  `limpet_presentation_decode` have new link names (`..._v3`); objects
  built with the 0.1 or 0.2 header fail to link instead of overflowing
  their struct. Source code is unchanged.
- `test_sdk_compat.sh` checks SDK 0.1.0 and SDK 0.2.0 clients (vendored in
  `sdk/tests/compat-0.2.0` from tag `sdk-v0.2.0`) against the current reply.

Protocol header `tty_control.h`: SHA-256
`80894ef8484a47852fa861cac26cecdc88b9d5bc5b3b41da40b33f0d7034e64b`.

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
