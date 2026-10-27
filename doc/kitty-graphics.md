# Kitty graphics subset

Limpet draws inline images sent through the terminal stream, using a
subset of the Kitty graphics protocol.

Programs find it through the presentation query: Limpet 0.8.0 sets
`XTY_FEATURE_KITTY_GRAPHICS` ([presentation.md](presentation.md)).

Source: [`src/kitty_graphics.h`](../src/kitty_graphics.h). APC
`ESC _ G <control> ; <payload> ESC \`.

| Feature | Support |
|---|---|
| Actions | `a=t` transmit, `a=T` transmit and display, `a=p` display, `a=d` delete, `a=q` query |
| Formats | `f=24` RGB, `f=32` RGBA (alpha blended onto the text beneath) |
| Transmission | `t=d` direct, base64, chunked `m=1` … `m=0` |
| Images | `i` id (required except for delete-all). One placement per image; displaying again moves it. Retransmitting an id replaces its pixels, and the placement stays. |
| Delete | `d=a/A` all (A also frees the images), `d=i/I` by id (I also frees) |
| Replies | `ESC _ G i=<id>;OK ESC \` or `…;E<CODE>:<text>`, none without an id; `q=1` suppresses OK and `q=2` suppresses everything |
| Cursor | `C=0` (default) puts the cursor on the image's last row, one column past its right edge (last column with a wrap pending when that is past the edge); when the image would pass the bottom margin the screen scrolls up first, so the image shows whole (0.3.0; full-screen margins only). `C=1` never moves the cursor or scrolls |
| Clearing | `CSI 2 J` removes the placements on the screen shown; entering the alternate screen removes that screen's placements (0.3.0). The images stay transmitted |
| Unsupported | JPEG/PNG decoding, files, shared memory, compression, animation frames, image numbers, extra placements, other delete targets, unicode placeholders, cropping and scaling keys: all answered with `ENOTSUPPORTED` |

## Transmission and display

- **Replacement:** A new image replaces the old one only when its last chunk
  arrives with an exact byte count. An interrupted or malformed transmission
  (CAN/SUB, a stray ESC, bad base64, too much or too little data, another
  command in between) is dropped and its buffer freed; the previous image
  stays.
- **Invalid input:** Unsupported commands and bad data never
  appear as text: the model routes every APC string to the sink or discards
  it.
- **Limits:** 1024x1024 pixels, 4 MiB decoded per image, 16 MiB for all
  images plus the open transmission, 16 images, 512 bytes of control data.
- **Reply handling:** A string shaped like a reply (only `i`, `I` and `p`
  keys) is ignored silently. In a cooked tty the terminal's own reply is
  echoed back to it; answering the echo would start a reply loop.
- **Rendering:** Images are composited into the row pixel buffer before
  `WritePixelArray`, so text and image are drawn in one pass. Rows an image
  covers or covered are damaged when it changes, moves, scrolls or is
  deleted. The image is clipped to the grid on resize.
- **Visibility:** Images are hidden while the scrollback view is scrolled
  back and in the command view.
- **Position:** An image is anchored to the text row it was placed on: it
  scrolls with the text, and a window resize keeps it on that row (exact
  for a height change; after a width change it follows the screen's old
  top row, so it can be a row or so off). Transmit, query and delete never
  move the cursor or scroll.

Implementation and regression tests: `src/kitty_graphics.c` and
`tests/test_kitty_graphics.c`.
