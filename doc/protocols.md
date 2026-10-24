# Protocol reference

This reference covers **Limpet 0.7.0**. Applications use escape sequences
through standard I/O and the control protocol through DOS packets on the same
handle. See the [capability reference](capabilities.md) for feature detection
and the version of each feature.

All escape sequences below are 7-bit: `ESC` is 0x1B, `CSI` means `ESC [`,
`OSC` means `ESC ]`, `APC` means `ESC _`, `ST` means `ESC \`. The 8-bit C1
forms (0x9B and so on) are not accepted on output.

## Output: control characters

| Byte | Effect |
| --- | --- |
| LF (0x0A) | next line, scrolling at the bottom margin; in a Shell session in cooked mode the cursor also returns to column 1, as in `CON:`; in raw mode LF only moves down |
| CR (0x0D) | column 1 |
| BS (0x08) | one column left |
| HT (0x09) | next multiple of 8 columns |
| FF (0x0C) | clear the screen, cursor home |
| BEL (0x07) | ignored, except as an OSC terminator |

Text is UTF-8. An invalid byte is shown as U+FFFD.

## Output: CSI sequences

| Sequence | Name | Notes |
| --- | --- | --- |
| `CSI n A` / `B` / `C` / `D` | cursor up, down, forward, back | default 1 |
| `CSI n E` / `F` | next / previous line, column 1 | no scrolling |
| `CSI n G`, ``CSI n ` `` | column | |
| `CSI n d` | row | |
| `CSI r ; c H`, `CSI r ; c f` | position | one-based |
| `CSI n J` | erase in display | 0, 1, 2 |
| `CSI n K` | erase in line | 0, 1, 2 |
| `CSI n L` / `M` | insert / delete lines | inside the scrolling region |
| `CSI n S` / `T` | scroll up / down | inside the scrolling region |
| `CSI t ; b r` | scrolling region | moves the cursor home |
| `ESC M` | reverse index | |
| `CSI ? 25 h` / `l` | show / hide cursor | |
| `CSI ? 1049 h` / `l` | alternate screen on / off | primary screen and cursor restored |
| `CSI ? 1000 h`, `? 1002 h`, `? 1006 h` | mouse reporting | raw mode only |
| `CSI ? 2004 h` / `l` | bracketed paste | **0.2.0**; raw mode only |
| `CSI ? 2048 h` / `l` | resize reports | **0.2.0**; raw mode only |
| `CSI ? 2100 h` / `l` | wheel reports, buttons left to the terminal | **0.2.0**; Limpet private mode; raw mode only |
| `CSI > 4 ; 1 m` / `CSI > 4 m` | Shift+Enter as `CSI 27 ; 2 ; 13 ~` on / off (xterm modifyOtherKeys, Enter only); also off: `CSI > 4 ; 0 m`, `CSI > 4 n`, `CSI > m` | **0.6.0**; raw mode only |
| `CSI ... m` | SGR | see below |
| `CSI 5 n`, `CSI 6 n`, `CSI ? 6 n` | status, cursor position | reply in raw mode only |
| `CSI 18 t` | text area size | reply `CSI 8 ; rows ; cols t`, raw mode only |
| `0x9B 6 n` | cursor position, Amiga form | **0.6.0**; reply `0x9B row ; col R`, raw mode only |
| `0x9B 0 SPACE q`, `0x9B SPACE q` (8-bit CSI) | window bounds, Amiga form | **0.6.0**; reply `0x9B 1 ; 1 ; rows ; cols SPACE r`, raw mode only. With `ESC [` it is xterm's cursor style and gets no reply since 0.6.1 |

Other `CSI >` sequences are ignored. Anything else is ignored. In
particular there is no reply to device
attribute requests (`CSI c`), so a program cannot identify Limpet that way.

## Output: SGR

| Code | Effect |
| --- | --- |
| 0 | reset |
| 1, 22 | bold on, off |
| 4, 24 | underline on, off |
| 7, 27 | inverse on, off |
| 30-37, 40-47 | colour 0-7, **or Workbench pen 0-7 when `XTY_COLOR_PENS` is reported** |
| 90-97, 100-107 | bright colour 8-15 |
| 38;5;n, 48;5;n | indexed colour 0-255 |
| 38;2;r;g;b, 48;2;r;g;b | 24-bit colour |
| 39, 49 | theme default foreground, background |

Colon forms (`38:2::r:g:b`) and all other codes are ignored. An incomplete
38/48 sequence stops parsing of that SGR, so its numbers are never read as
other attributes.

## Output: OSC

OSC 8 and, from 0.3.0, OSC 0 and OSC 2 are interpreted; BEL also
terminates. `OSC 8 ; params ; URI ST` opens a link, `OSC 8 ; ; ST` closes
it; details and bounds in [hyperlinks.md](hyperlinks.md).
`OSC 0 ; text ST` and `OSC 2 ; text ST` set the window title while the
command that sent them runs: 1 to 255 bytes of valid UTF-8 without control
characters, else the whole sequence is ignored; empty text drops the
program's title. Detection, lifetime and display:
[capabilities.md](capabilities.md#window-title). Every other OSC (icon
name, palette, clipboard) is consumed and ignored.

## Output: APC

`APC G keys ; payload ST` is the Kitty graphics subset; see
[kitty-graphics.md](kitty-graphics.md). Other APC strings are consumed and
ignored. Image data never appears as text.

## Input: what a program reads

| Key | Raw mode bytes |
| --- | --- |
| cursor keys | `CSI A`, `CSI B`, `CSI C`, `CSI D` |
| Ctrl-C | 0x03 (in cooked mode: the break signal instead) |
| Ctrl-D, Ctrl-E, Ctrl-F | 0x04, 0x05, 0x06 (in cooked mode, from 0.6.0: the break signals D, E, F instead) |
| Ctrl-\ | 0x1C (in cooked mode, from 0.6.0: end of input once instead) |
| Esc | 0x1B |
| Enter | 0x0D |
| Shift+Enter | 0x0D; `CSI 27 ; 2 ; 13 ~` after `CSI > 4 ; 1 m` (0.6.0) |

Replies to queries (`CSI 6 n`, `CSI 18 t`, Kitty `a=q`) arrive in the same
stream, in raw mode only. Mouse reports (`CSI < b ; x ; y M`) likewise.

| Input from Limpet | Bytes | When |
| --- | --- | --- |
| bracketed paste | `CSI 200 ~` text `CSI 201 ~` | **0.2.0**, mode 2004 on |
| resize report | `CSI 48 ; rows ; cols ; height ; width t` | **0.2.0**, mode 2048 on: at once, then after each change |
| wheel step | `CSI < 64 ; col ; row M` (up), `CSI < 65 ; col ; row M` (down), +16 Ctrl, +8 Alt | **0.2.0**, mode 2100 on and 1000/1002 off |

### Order of input (0.2.0)

- Nothing goes inside a bracketed paste: from the first byte of
  `CSI 200 ~` to the last byte of `CSI 201 ~`, keys, mouse and wheel
  reports, query replies and resize reports wait.
- A report or reply is never cut by other input, however few bytes the
  program reads at a time.
- A paste cancelled with Esc still ends with `CSI 201 ~`. A new paste
  starts only after the previous one has been delivered.
- When the program leaves raw mode or the session ends, an undelivered
  bracketed paste and a pending resize report are dropped.

### Modes end with raw mode (0.2.0)

When a native Shell window returns to cooked mode, Limpet turns off 1000,
1002, 1006, 2004, 2048, 2100 and (0.6.0) the Shift+Enter mode. Set them again after each
`SetMode(Input(), 1)`. A PTY session (experimental) resets them when it
ends.

`CON:` sends cursor keys as 0x9B followed by the letter. Programs that run
in both consoles decode both.

## DOS packets: public console geometry

Limpet 0.7.0 and later answer `ACTION_CONSOLE_GETGEOMETRY` (`0x434f4e47`),
following the proposed AROS `dos/consoleinfo.h` contract in
[PR1763](https://github.com/aros-development-team/AROS/pull/1763), checked at
`4fd9618699101fa6425146d7d0c0eb7767fe23cf`. The upstream PR is still open.
This is independent of Limpet's private control protocol below.

Pass the handle's `fh_Arg1` in Arg1, a writable output pointer in Arg2, its byte
capacity in Arg3, and version 1 in Arg4. Success returns `DOSTRUE`, Res2 zero,
and copies six 32-bit words: size (24), version (1), columns, rows, width and
height. Pixel dimensions cover complete character cells, excluding window
borders, margins and partial cells. The result contains no handler-owned pointers.

Both the Shell/mounted handler and the renderer-backed PTY endpoint answer the
query. A short buffer, null pointer or unsupported version returns
`ERROR_BAD_NUMBER` without changing the buffer. A stale/invalid handle returns
`ERROR_INVALID_LOCK`; an endpoint without a renderer returns
`ERROR_ACTION_NOT_KNOWN`. A larger buffer is accepted; bytes after the snapshot
are untouched. This query neither consumes input nor subscribes to resize events.
Applications must tolerate `ERROR_ACTION_NOT_KNOWN` from older Limpet builds and
other console handlers.

## DOS packets: the control protocol

### Envelope

| Packet field | Value |
| --- | --- |
| `dp_Type` | `XTY_ACTION_CONTROL` (0x58545901) |
| `dp_Arg1` | the handle's `fh_Arg1` |
| `dp_Arg2` | pointer to a 64-byte `XtyControl` block |
| `dp_Arg3` | 64 |
| `dp_Arg4` | `XTY_PACKET_MAGIC` (0x58545931) |
| `dp_Arg5` | 0 |

Send it to the handle's `fh_Type` port. The block:

```c
typedef struct {
    uint32_t version, operation, reserved[2], values[12];
} XtyControl;   /* 64 bytes, native endian, no pointers */
```

Request: `version` 1, `operation`, `reserved` both 0, `values` as the
operation needs. A successful reply returns `DOSTRUE` with error 0, the
same version and operation, and `reserved[0]` set to `XTY_PACKET_MAGIC`.
The response marker distinguishes a completed request from an unchanged block. On failure the block is unchanged and the reply is
`DOSFALSE` with an error code.

### Control operations

| No. | Name | Released Shell session (through 0.6.0) | Experimental PTY |
| --- | --- | --- | --- |
| 1 | `XTY_CAPS` | `ERROR_ACTION_NOT_KNOWN` | yes |
| 2-12 | config, geometry, read state, flush, drain, flow | `ERROR_ACTION_NOT_KNOWN` | yes |
| 13 | `XTY_GET_PRESENTATION` | **yes** | yes, when attached to the renderer |

`XTY_ACTION_CANCEL` is accepted by the Shell session from Limpet 0.4.1, for a
client's own pending read only (no capability bit; a refusal means not
supported). Operation 13 is described in [presentation.md](presentation.md). Operations
1 to 12, `XTY_ACTION_CANCEL` and the immediate I/O actions are described in
the header and in [the experimental guide](sdk/experimental.md).

### Errors

| Code | When |
| --- | --- |
| `ERROR_ACTION_NOT_KNOWN` (209) | the console does not implement the action or the operation; a Limpet session that is closing |
| `ERROR_BAD_NUMBER` (115) | wrong `dp_Arg3`, `dp_Arg4` or `dp_Arg5`, a NULL block, or a request with nonzero fields the operation does not use |
| `ERROR_INVALID_LOCK` (211) | a handle Limpet did not open |

## Compatibility rules

- Numbers of packet types, operations and bits never change meaning.
- New fields use reserved values that are zero today. Clients ignore
  values and bits they do not know.
- A clear bit in the known-field mask means unknown, not unsupported.
- The block stays 64 bytes. A future layout would use a new `version`, and
  a terminal would answer an unknown version with an error, never with a
  reinterpreted block.
