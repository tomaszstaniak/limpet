# Limpet SDK changes

## Unreleased

- `LimpetPresentation.input`, appended: the input features a raw-mode
  client may turn on, valid with `XTY_PRESENT_KNOWN_INPUT` (bit 32).
  `XTY_INPUT_BRACKETED_PASTE`: DEC mode 2004; `XTY_INPUT_RESIZE_REPORT`:
  DEC mode 2048. Zero from Limpet 0.1.0.

## 0.1.0 (2026-10-05)

First release.

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
