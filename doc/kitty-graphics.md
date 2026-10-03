# Kitty graphics subset

Limpet draws inline images sent through the terminal stream, using a
subset of the Kitty graphics protocol.


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
| Cursor | `C=0` moves the cursor past the image, `C=1` leaves it |
| Unsupported | PNG, files, shared memory, compression, animation frames, image numbers, extra placements, other delete targets, unicode placeholders, cropping and scaling keys: all answered with `ENOTSUPPORTED` |

Rules:

- **Commit.** A new image replaces the old one only when its last chunk
  arrives with an exact byte count. An interrupted or malformed transmission
  (CAN/SUB, a stray ESC, bad base64, too much or too little data, another
  command in between) is dropped and its buffer freed; the previous image
  stays.
- **Nothing reaches the grid.** Unsupported commands and bad data never
  appear as text: the model routes every APC string to the sink or discards
  it.
- **Limits.** 1024x1024 pixels, 4 MiB decoded per image, 16 MiB for all
  images plus the open transmission, 16 images, 512 bytes of control data.
- **Echo guard.** A string shaped like a reply (only `i`, `I` and `p`
  keys) is ignored silently. In a cooked tty the terminal's own reply is
  echoed back to it; answering the echo would start a reply loop.
- **Drawing.** Images are composited into the row pixel buffer before
  `WritePixelArray`, so text and image are drawn in one pass. Rows an image
  covers or covered are damaged when it changes, moves, scrolls or is
  deleted. The image is clipped to the grid on resize.
- **Visibility.** Images are hidden while the scrollback view is scrolled
  back and in the command view.

Host tests: `build/test_kitty_graphics` (in `make test`, also clean under
ASan/UBSan) covers:

- whole and chunked transmissions at every split, including one byte at a
  time;
- short, long and bad-base64 lengths;
- CAN, SUB, stray ESC and another command in the middle of an image;
- unsupported keys and every reply/quiet case;
- dimension, size, memory and count limits;
- replace and delete;
- the model sink;
- echoed replies.

