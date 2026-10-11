# Limpet SDK API reference

SDK version 0.2.0. Header: `#include <limpet/limpet.h>`, which also
includes the protocol header `limpet/tty_control.h`.

## Two layers

| Layer | File | What it is | Stability |
| --- | --- | --- | --- |
| Protocol | `tty_control.h` | `XtyControl`, packet types, operation numbers, bit values | Fixed. The terminal is built from this same file. |
| Convenience API | `limpet.h` | functions that build requests, validate replies and write sequences | SDK 0.x: may grow; see [versioning](../versioning.md) |

Everything in `limpet.h` can be written by hand from `tty_control.h`. The
mapping is shown for each function below, so that a program can use the
protocol directly or check what the SDK does.

## General rules

- **Handles.** Functions that take a `BPTR` use that DOS FileHandle and no
  other. Keep it open for the whole call. Do not use it from another task
  at the same time.
- **Memory.** No function allocates memory or keeps a pointer after it
  returns. Buffers belong to the caller.
- **Blocking.** Only `limpet_query_presentation()`, `limpet_control()` and
  `limpet_write_dos()` talk to DOS, and they block until the handler
  replies. There is no timeout. All other functions are pure computations.
- **Tasks.** The pure functions are reentrant. The DOS functions set
  `IoErr()` of the calling process.
- **Strings.** Builders always leave a NUL-terminated string. When they
  return 0, the buffer holds an empty string: write nothing.
- **C++.** The header has `extern "C"` guards.

## Version macros

```c
#define LIMPET_SDK_VERSION_MAJOR 0
#define LIMPET_SDK_VERSION_MINOR 3
#define LIMPET_SDK_VERSION_PATCH 0
#define LIMPET_SDK_VERSION_STRING "0.3.0"
```

These describe the SDK, not the terminal. The terminal's version is not
reported by any query; features are.

## Building with one SDK version

`LimpetPresentation` is part of the binary interface. SDK 0.2.0 appended
`input` (28 to 32 bytes) and SDK 0.3.0 `features` (32 to 36 bytes).
Compile every object that uses the SDK, and the library, with the same SDK
version.

To make a mix fail safely, `limpet.h` renames the two functions that fill
the struct:

```c
#define limpet_query_presentation limpet_query_presentation_v3
#define limpet_presentation_decode limpet_presentation_decode_v3
```

Source code keeps using the plain names. An object compiled with the 0.1 or
0.2 header refers to an old link name, so linking it against the 0.3
library fails with an undefined symbol instead of overwriting the caller's
stack.
`sdk/tests/test_sdk_compat.sh` checks that this link fails.

## Presentation query

### `LimpetStatus`

| Value | Meaning | What to do |
| --- | --- | --- |
| `LIMPET_OK` | Reply received and valid | decorate according to the reply |
| `LIMPET_NOT_INTERACTIVE` | Handle is a file, pipe or `NIL:`; no packet sent | plain text, no escape sequences |
| `LIMPET_UNSUPPORTED` | Handler answered `ERROR_ACTION_NOT_KNOWN` | plain text |
| `LIMPET_INVALID_REPLY` | Handler answered with a malformed reply | plain text; worth logging |
| `LIMPET_FAILED` | Another DOS error; code in `*ioerr` | plain text |
| `LIMPET_BAD_ARGUMENT` | A NULL pointer or handle | fix the call |

### `LimpetPresentation`

```c
typedef struct {
    uint32_t known;
    uint32_t color_level;
    uint32_t styles;
    uint32_t lines;
    uint32_t rows, columns;
    uint32_t interpretation;
    uint32_t input;          /* 0.2.0 */
    uint32_t features;       /* 0.3.0 */
} LimpetPresentation;
```

| Field | Valid when `known` has | Values |
| --- | --- | --- |
| `color_level` | `XTY_PRESENT_KNOWN_COLORS` (1) | `XTY_COLOR_NONE` 0, `ANSI16` 1, `INDEXED256` 2, `RGB` 3 |
| `styles` | `XTY_PRESENT_KNOWN_STYLES` (2) | bit `XTY_STYLE_BOLD_RESET` (1) |
| `lines` | `XTY_PRESENT_KNOWN_LINES` (4) | bits `XTY_LINE_CR` (1), `XTY_LINE_ERASE_EOL` (2) |
| `rows`, `columns` | `XTY_PRESENT_KNOWN_GEOMETRY` (8) | both positive |
| `interpretation` | `XTY_PRESENT_KNOWN_INTERPRETATION` (16) | bit `XTY_COLOR_PENS` (1) |
| `input` (0.2.0) | `XTY_PRESENT_KNOWN_INPUT` (32) | bits `XTY_INPUT_BRACKETED_PASTE` (1), `XTY_INPUT_RESIZE_REPORT` (2), `XTY_INPUT_WHEEL_REPORT` (4): modes a raw-mode program may turn on |
| `features` (0.3.0) | `XTY_PRESENT_KNOWN_FEATURES` (64) | bit `XTY_FEATURE_TITLE` (1): OSC 0/2 set the window title (Limpet 0.3.0) |

**Unknown versus unsupported.** A clear bit in `known` means the terminal
said nothing about that field. A set bit with a zero value means the
feature is confirmed absent. The decoder sets fields with a clear bit to
zero, so a client that forgets to test a bit sees "nothing", never stale
data.

**Future fields.** `known` keeps bits this SDK does not know. Unknown bits
in `styles`, `lines` and `interpretation` are kept too. Test only the bits
you know.

### `limpet_query_presentation`

```c
LimpetStatus limpet_query_presentation(BPTR output, LimpetPresentation *out,
                                       LONG *ioerr);
```

Asks the console behind `output` what it renders.

- `output`: the handle you write to, normally `Output()`.
- `out`: filled on `LIMPET_OK`, zeroed otherwise.
- `ioerr`: may be NULL; receives the DOS error code, 0 on success.

Steps: if `output` is not interactive, returns `LIMPET_NOT_INTERACTIVE`
without sending anything. Otherwise sends operation 13 through
`limpet_control()` and decodes the reply with
`limpet_presentation_decode()`.

Blocks until the handler replies. Does not read input, does not change
modes, does not allocate.

Flush stdio first (`fflush(stdout)`) when the answer decides how to print
text that follows: the packet is not ordered behind data still in a C
library buffer.

**By hand:** zero an `XtyControl`, set `version = XTY_CONTROL_VERSION` (1)
and `operation = XTY_GET_PRESENTATION` (13), and send
`DoPkt(fh->fh_Type, XTY_ACTION_CONTROL, fh->fh_Arg1, &block, 64,
XTY_PACKET_MAGIC, 0)` where `fh = BADDR(output)`. See
[presentation.md](../presentation.md) for the reply layout.

### `limpet_presentation_request`

```c
void limpet_presentation_request(XtyControl *request);
```

Zeroes the block and sets version 1 and operation 13. Every other field
must be zero on the wire; Limpet rejects a request with anything else in
it (`ERROR_BAD_NUMBER`).

### `limpet_presentation_decode`

```c
LimpetStatus limpet_presentation_decode(const XtyControl *reply,
                                        LimpetPresentation *out);
```

Checks a received block: version 1, operation 13, `reserved[0]` equal to
`XTY_PACKET_MAGIC`, `reserved[1]` zero, and both sizes positive when the
geometry bit is set. Returns `LIMPET_OK` or `LIMPET_INVALID_REPLY`.
`values[7]` is decoded into `input` when known bit 32 is set;
`values[8]` to `values[11]` are ignored, not rejected. Pure; usable in host
tests.

### `limpet_control`

```c
LONG limpet_control(BPTR handle, XtyControl *control);
```

The transport under the query. Sends one control block and validates the
envelope of the reply.

- Returns `DOSTRUE` and copies the reply into `*control` when the reply
  has the same version and operation and carries `XTY_PACKET_MAGIC` in
  `reserved[0]`.
- Returns `DOSFALSE` and sets `IoErr()` otherwise; `*control` is unchanged.

| `IoErr()` | Cause |
| --- | --- |
| `ERROR_ACTION_NOT_KNOWN` (209) | the handler does not implement the action or the operation; also a handler that refused without a code, or returned the block without answering |
| `ERROR_BAD_NUMBER` (115) | NULL argument, a request Limpet rejected, or a reply that breaks the packet rules |
| `ERROR_INVALID_LOCK` (211) | the handle has no handler (`NIL:`, closed) |
| other | passed through from the handler |

A Limpet Shell session (0.1.0 and 0.2.0) answers only operation 13. Every
other operation returns `ERROR_ACTION_NOT_KNOWN`.

## Decisions

All take a `const LimpetPresentation *` (NULL allowed, treated as
"nothing known") and never fail.

| Function | Returns |
| --- | --- |
| `uint32_t limpet_color_level(p)` | the colour level, or `XTY_COLOR_NONE` if unknown or newer than this SDK |
| `int limpet_uses_pens(p)` | 1 if SGR 30-37 select Workbench pens (known bit and `XTY_COLOR_PENS`) |
| `int limpet_has_bold(p)` | 1 if SGR 1 and SGR 0 are confirmed |
| `int limpet_can_rewrite_line(p)` | 1 if CR and `CSI K` are both confirmed |
| `uint32_t limpet_columns(p)` | columns, or 0 if unknown |
| `uint32_t limpet_rows(p)` | rows, or 0 if unknown |
| `int limpet_can_set_title(p)` | 1 if OSC 0/2 window titles are confirmed (0.3.0; Limpet 0.2.x answers without confirming) |

### Input modes (0.2.0)

There are no helper functions for these; test the bits and send the mode
sequences yourself, after switching to raw mode:

```c
int paste = (p.known & XTY_PRESENT_KNOWN_INPUT) && (p.input & XTY_INPUT_BRACKETED_PASTE);
int resize = (p.known & XTY_PRESENT_KNOWN_INPUT) && (p.input & XTY_INPUT_RESIZE_REPORT);

if (SetMode(Input(), 1)) {
    if (paste) fputs("\033[?2004h", stdout);
    if (resize) fputs("\033[?2048h", stdout);
    fflush(stdout);
    /* ... read input: CSI 200 ~ text CSI 201 ~, CSI 48 ; r ; c ; h ; w t ... */
    if (paste) fputs("\033[?2004l", stdout);
    if (resize) fputs("\033[?2048l", stdout);
    fflush(stdout);
    SetMode(Input(), 0);
}
```

Limpet also turns the modes off when the console returns to cooked mode,
so a program that stops early does not leave them on. What each mode
delivers: [capability catalogue](../capabilities.md#bracketed-paste).

## Sequence builders

Each writes into `buf` (capacity `size`, including the NUL) and returns the
length without the NUL, or 0 when nothing should be written: the feature
is not confirmed, an argument is out of range, or the buffer is too small.
A sequence is never truncated.

### `limpet_sgr_ansi`

```c
size_t limpet_sgr_ansi(char *buf, size_t size, const LimpetPresentation *p,
                       unsigned index, int background);
```

One of the 16 ANSI colours, `index` 0 to 15, keeping its ANSI meaning.
Needs colour level ANSI16 or higher.

| Pens | Foreground | Background |
| --- | --- | --- |
| off | `ESC[30m`..`ESC[37m`, `ESC[90m`..`ESC[97m` | `ESC[40m`..`ESC[47m`, `ESC[100m`..`ESC[107m` |
| on | `ESC[38;5;nm` | `ESC[48;5;nm` |

`LIMPET_SEQUENCE_MAX` (32) is enough for any SGR builder.

### `limpet_sgr_rgb`

```c
size_t limpet_sgr_rgb(char *buf, size_t size, const LimpetPresentation *p,
                      unsigned r, unsigned g, unsigned b, int background);
```

At level RGB: `ESC[38;2;r;g;bm`. At level INDEXED256: the nearest entry of
the 6x6x6 cube, `ESC[38;5;nm` with n from 16 to 231. Below that: nothing,
because only the caller knows which of the 16 colours is an acceptable
substitute. Components above 255 return 0.

### Fixed sequences

| Macro | Bytes | Effect |
| --- | --- | --- |
| `LIMPET_SGR_RESET` | `ESC[0m` | all attributes off |
| `LIMPET_SGR_BOLD` | `ESC[1m` | bold |
| `LIMPET_SGR_FG_DEFAULT` | `ESC[39m` | theme foreground, bold kept |
| `LIMPET_SGR_BG_DEFAULT` | `ESC[49m` | theme background |
| `LIMPET_ERASE_EOL` | `ESC[K` | erase to end of line |
| `LIMPET_OSC8_CLOSE` | `ESC]8;;ESC\` | end a link |

Send them only after a successful query, like everything else.

### `limpet_osc8_open`

```c
size_t limpet_osc8_open(char *buf, size_t size, const char *uri);
```

`ESC ] 8 ; ; uri ESC \`. The URI must be 1 to 2047 bytes
(`LIMPET_OSC8_URI_MAX`) of printable ASCII, 0x21 to 0x7E. Spaces, controls
and non-ASCII bytes return 0: percent-encode them. Limpet makes only
`http://` and `https://` targets clickable.

### `limpet_osc_title` (0.3.0)

```c
#define LIMPET_TITLE_MAX 255
#define LIMPET_TITLE_RESET "\033]2;\033\\"
size_t limpet_osc_title(char *buf, size_t size, const LimpetPresentation *p,
                        const char *text);
```

`ESC ] 2 ; text ESC \`, for a terminal that confirmed `XTY_FEATURE_TITLE`
only. `text` is 1 to 255 bytes of valid UTF-8 with no control characters
(C0, DEL, C1); anything else returns 0. An empty `text` builds
`LIMPET_TITLE_RESET`, which drops the program's title. A buffer of
`LIMPET_TITLE_MAX + 7` bytes always fits.

Limpet keeps the title while the command that sent it runs and drops it
when that command ends, so a program does not have to reset it on exit.
Send a new title when the program's state changes, not with each line of
output: Limpet applies at most about four changes a second. The title bar
shows Latin-1; other characters appear as `?`. Example:
[`title`](../../sdk/examples/title.c); behaviour:
[window title](../capabilities.md#window-title).

## Kitty graphics

Limits as constants: `LIMPET_KITTY_MAX_SIDE` (1024 pixels),
`LIMPET_KITTY_MAX_BYTES` (4 MiB of decoded pixels per image),
`LIMPET_KITTY_MAX_IMAGES` (16 per terminal), `LIMPET_KITTY_CHUNK`
(4096 base64 bytes per escape sequence).

### `LimpetWriteFn`, `limpet_write_dos`

```c
typedef int (*LimpetWriteFn)(void *context, const char *bytes, size_t length);
int limpet_write_dos(void *context, const char *bytes, size_t length);
```

The image functions write through a callback, so they work with stdio, DOS
handles or your own buffer. Return 0 when all bytes were written, nonzero
to stop. `limpet_write_dos` takes a `BPTR` cast to `void *`, retries short
writes and returns 1 on a failed `Write()`. Do not mix it with buffered
stdio output on the same handle without `fflush()` in between.

### `LimpetImage`, `limpet_kitty_transmit`

```c
typedef struct {
    uint32_t id;
    unsigned width, height;
    unsigned format;      /* 24 or 32 */
    int keep_cursor;      /* 1: C=1 */
    unsigned quiet;       /* 0, 1 or 2 */
} LimpetImage;

int limpet_kitty_transmit(const LimpetImage *image, const unsigned char *pixels,
                          LimpetWriteFn write, void *context);
```

Sends `a=T` (transmit and display at the cursor) with direct base64 data in
chunks, one complete escape sequence per callback. `pixels` holds
`width * height * format / 8` bytes, rows from top to bottom.

Returns 0, -1 for invalid arguments (nothing written: id 0, size out of
range, format other than 24 or 32, quiet above 2, more than 4 MiB), or the
callback's nonzero value. After a failed callback past the first chunk,
the function tries to send an empty final chunk so that the terminal drops
the incomplete image; the previous image with that id stays.

Reusing an id replaces the image. Use `quiet = 2` unless you read replies
in raw mode.

### `limpet_kitty_delete`

```c
int limpet_kitty_delete(uint32_t id, int free_data,
                        LimpetWriteFn write, void *context);
```

Removes the image's placement (`d=i`), and with `free_data` also its pixels
(`d=I`). `id` 0 removes every placement (`d=a`, or `d=A` with
`free_data`). Sends with `q=2`.

### `limpet_kitty_query`, `limpet_kitty_parse_reply`

```c
size_t limpet_kitty_query(char *buf, size_t size, uint32_t id);
int limpet_kitty_parse_reply(const char *bytes, size_t length, uint32_t id,
                             char *message, size_t message_size);
```

The query is a 1x1 RGB `a=q` with the given id. Write it in raw mode, read
up to `ESC \` with a time limit, and parse:

| Return | Meaning |
| --- | --- |
| 1 | `OK`: the terminal supports the subset |
| 0 | an error reply; its text (for example `ENOTSUPPORTED:...`) is copied to `message` |
| -1 | no reply for this id in the bytes |

Bytes before `ESC _ G` (an early key press) are skipped, not consumed: keep
them if your program needs them.

## Building against the SDK

- Include path: `include/`.
- Link: `-Llib/abiv11 -lLimpet`, or compile `src/limpet_query.c` and
  `src/limpet_format.c` with your program.
- Requirements: `dos.library` and the C library. No other library is
  opened.
