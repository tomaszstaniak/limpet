# Verification record: Limpet 0.3.0 release candidate

Limpet 0.3.0 adds a window title that names the running command and its
state, Ctrl-R history search, one prompt after Ctrl-C, inline images that
make room at the bottom edge, and three fixes (title buffer, selection,
images on resize). SDK 0.3.0 adds the title feature bit and
`limpet_osc_title()`. This record says what was checked, where, and what
was not. The candidate is not published.

## Build

| Item | Value |
| --- | --- |
| Sources | branch `limpet-0.3`, the commit that adds this record; the program's sources are those of commit `ea8dbab` |
| Build | `make abiv11-frontend LIMPET_VERSION=0.3.0 LIMPET_DATE=11.10.2026`, `x86_64-aros-gcc` 10.5.0, ABIv11 2026.09 Developer SDK |
| Program | SHA-256 `784a0c88726929d771395bb6926e493aa2b627859eff83ee19831512f3b0ed71`, the same bytes from a clean checkout and from the working tree |
| Size | 3,375,824 bytes; 0.2.1 was 3,357,536 (+18,288 bytes, +0.54 %) |
| Archive | `Limpet-0.3.0-abiv11.zip`, 3,648,704 bytes, SHA-256 `99577cfff6aa25cd5faa5abfa202d0c2498edce30459abba3734de7f64a6fa8b`. Against 0.2.1 only `Limpet/Limpet` and `Limpet/README.txt` differ |

## Host tests

`make test` passes, including `test-sdk` (contract tests under ASan and
UBSan, the C and C++ header check, and the compatibility test for SDK
0.1.0 and 0.2.0 clients). New and extended:

- `tests/test_osc_title.c`: OSC 0/2 at every split of the bytes, BEL and
  ST, UTF-8, the 255-byte limit, empty text, controls, C1, malformed and
  overlong UTF-8 dropped whole, other OSC ignored, links unaffected;
- `tests/test_window_title.c`: the order link, program, command, Shell;
  status after the name; Latin-1 mapping; truncation; how long a
  program's title lasts;
- `tests/test_console_input.c`: when Ctrl-C completes with an empty line;
- `tests/test_kitty_graphics.c`: cursor and scrolling at the bottom and
  right edges, `C=1`, partial scroll regions, an image taller than the
  window, clearing, the alternate screen, scrollback, resize with 40
  lines of history (taller, shorter, narrower, wider);
- `tests/test_line_editor.c`: Ctrl-R start, narrowing, case, repeat with
  duplicates skipped, a visible and harmless failure, Backspace, Esc,
  Enter without running, Unicode, the 32-entry limit, the label bounds;
- `sdk/tests/test_sdk.c`: the feature bit, a 0.2.x reply without it,
  `limpet_osc_title()` validation.

`test_kitty_graphics` and `test_osc_title` also ran under AddressSanitizer and
UndefinedBehaviorSanitizer.

## The two reports, reproduced on 0.2.1 first

AROS One 1.3 (ABIv11, x86_64), QEMU TCG, published `Limpet-0.2.1-abiv11.zip`,
2026-10-11, with two probes built from `tests/` (`make abiv11-probes`):

- **Ctrl-C.** Interrupting `Wait 30` or a program that checks the break
  signal left `10.AROS:> 10.AROS:>`: two prompts on one line. Ctrl-C at an
  empty prompt gave `^C` and one prompt. In raw mode the program got byte
  03 and no signal. The system console showed one prompt after a break and
  nothing at an empty prompt. Cause: the session queued an empty input
  line with every break; while a command ran, the Shell read it after its
  next prompt. Source: the session, not the Shell, the echo or the drawing.
- **Images.** A 96-pixel image placed with `C=0` on the last row showed
  about two of its six rows; the text after it went beside the cut image
  and the prompt over it. With `C=1` the cursor stayed and the image was
  cut (as `C=1` asks). `clear` left images behind.

## On AROS

AROS One 1.3 (ABIv11, x86_64), QEMU TCG, 2026-10-11. Development builds of
this branch, then the archive above unpacked to `RAM:`.

From the archive:

| Check | Result |
| --- | --- |
| `Version FULL` | `Limpet 0.3.0 (10/11/26)` |
| Start | a Shell in the window, title `Shell - Limpet` |
| Unicode | `Type` of a Polish and CJK line drawn |
| Copy | the line selected, copied, pasted into `Echo >RESULTS:...`; the file equals the source byte for byte |
| Title | `Wait - Limpet` during `Wait 30`; the `title` example's states, then `Shell - Limpet` |
| Ctrl-C | one prompt after interrupting `Wait 30`; `^C` and one prompt at an empty prompt |
| Ctrl-R | `(reverse-i-search)` with the match; Enter put `Version ...` on the line without running it |
| Image | 96-pixel `C=0` image on the last row shown whole, text beside its last row; after resizing (shorter, taller, narrower, wider, with history above) still on its text row |
| hardcode | title `Hardcode - Limpet`, then `Hardcode: replying - Limpet` while a reply streamed, `Shell - Limpet` after `/quit` |
| Close | `EndCLI` ended the session (status 0); the close gadget closed the window; `Status` shows no Limpet or hardcode process |

From development builds of the same branch (earlier in the day):

| Check | Result |
| --- | --- |
| Command that fails to start | `nosuchcommand: object not found`, title back to `Shell` |
| Link hover | `Link: https://github.com/tomaszstaniak/limpet - Limpet`, back when the pointer left |
| Ctrl-C | a program checking the break: caught, one prompt; raw mode: byte 03, no signal; the next command ran |
| Ctrl-R in raw mode | byte 0x12 delivered to the program, no search |
| Ctrl-R | narrowing, an older different match, Esc back to the typed line with the cursor where it was, `(failed reverse-i-search)` keeping the line, a match accepted and edited before running, a Unicode command found with an ASCII query |
| Images | a fitting image scrolls nothing; `C=1` at the bottom unchanged; scrollback hides images and they come back; the SDK `image` example, which reserves rows and uses `C=1`, unchanged |
| Messages | `Selected`, then `Copied UTF-8 (...)`, gone after about four seconds |
| hardcode | `Hardcode: waiting for the model` before the first byte of a reply; the same hardcode in Limpet 0.2.1 kept the title `Limpet`, and in `CON:` printed no sequence |

The hardcode used here is branch `limpet-title` (commit `aa259b6`) of
aros-hardcode, built with SDK 0.3.0, against a local scripted test server.
The states "running a tool" and "waiting for your answer" were not seen on
screen: the test server's tool turns finished faster than a screenshot.
They are covered by hardcode's host test.

## Observed, not changed

- In raw mode Limpet treats a line feed as a line feed only; the system
  console also returns to column 0. A program that prints `\n` in raw mode
  continues in the same column in Limpet. Not part of this work.

## Not checked

ABIv11 2026.09 (the earlier releases' test system) with this candidate,
mainline AROS (no build of this version), real hardware, the mounted
console handler, the SDK examples `colors`, `progress`, `geometry` and
`keys` with SDK 0.3.0 (unchanged), any example in `CON:` with SDK 0.3.0,
Ctrl-R with a query typed in a non-ASCII character on AROS (the keyboard
injection cannot type one; host-tested), and speed.
