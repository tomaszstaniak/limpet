# Limpet changes

Changes of the terminal. The SDK has its own list in
[sdk/CHANGES.md](../sdk/CHANGES.md).

## 0.2.0 (2026-10-07)

For users:

- A selection stays while a program redraws other rows: a status line, a
  clock, a reply streaming below it. Output that changes or scrolls the
  selected text still clears it. Works during a drag, including selections
  larger than 1 KB.
- Erasing a whole row no longer joins it to the row above when the width
  changes later, so a redrawn status panel leaves no joined lines in the
  history.

For programs in raw mode, announced by the presentation query (known bit
32, `values[7]`):

- Bracketed paste, DEC mode 2004: a paste arrives as `CSI 200 ~` text
  `CSI 201 ~`, without the multi-line confirmation. Nothing else is put
  inside the frame; a paste cancelled with Esc still gets its end marker.
- Resize reports, DEC mode 2048: `CSI 48 ; rows ; cols ; height ; width t`
  at once and after each change.
- Wheel reports, private mode 2100: wheel steps as SGR mouse reports while
  buttons and drags stay with the terminal.
- When the console returns to cooked mode, these modes and mouse tracking
  1000/1002/1006 are turned off.

Protocol: version 1 unchanged; known bit 32 and `values[7]` are new, and
0.1.0 clients ignore both.

## 0.1.0 (2026-10-03)

First release, for AROS ABIv11 x86_64.
