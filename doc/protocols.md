# Protocol reference

Two channels lead from a program to Limpet:

1. **The byte stream**: what a program writes to its output handle, and
   what it reads back in raw mode. Escape sequences live here.
2. **DOS packets** on the same handle: the control protocol, of which
   Limpet 0.1.0 serves one operation.

This page lists both. For whether a feature can be detected and how far it
was checked, see the [capability catalogue](capabilities.md).

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
| `CSI ... m` | SGR | see below |
| `CSI 5 n`, `CSI 6 n`, `CSI ? 6 n` | status, cursor position | reply in raw mode only |
| `CSI 18 t` | text area size | reply `CSI 8 ; rows ; cols t`, raw mode only |

Anything else is ignored. In particular there is no reply to device
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

Only OSC 8 is interpreted: `OSC 8 ; params ; URI ST` opens a link,
`OSC 8 ; ; ST` closes it. BEL also terminates. Every other OSC (window
title, palette, clipboard) is consumed and ignored. Details and bounds:
[hyperlinks.md](hyperlinks.md).

## Output: APC

`APC G keys ; payload ST` is the Kitty graphics subset; see
[kitty-graphics.md](kitty-graphics.md). Other APC strings are consumed and
ignored. Image data never appears as text.

## Input: what a program reads

| Key | Raw mode bytes |
| --- | --- |
| cursor keys | `CSI A`, `CSI B`, `CSI C`, `CSI D` |
| Ctrl-C | 0x03 (in cooked mode: the break signal instead) |
| Esc | 0x1B |
| Enter | 0x0D |

Replies to queries (`CSI 6 n`, `CSI 18 t`, Kitty `a=q`) arrive in the same
stream, in raw mode only. Mouse reports (`CSI < b ; x ; y M`) likewise.

`CON:` sends cursor keys as 0x9B followed by the letter. Programs that run
in both consoles decode both.

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
The magic is how a client tells a real answer from a handler that left the
block untouched. On failure the block is unchanged and the reply is
`DOSFALSE` with an error code.

### Control operations

| No. | Name | Limpet 0.1.0 Shell session | Experimental PTY |
| --- | --- | --- | --- |
| 1 | `XTY_CAPS` | `ERROR_ACTION_NOT_KNOWN` | yes |
| 2-12 | config, geometry, read state, flush, drain, flow | `ERROR_ACTION_NOT_KNOWN` | yes |
| 13 | `XTY_GET_PRESENTATION` | **yes** | yes, when attached to the renderer |

Operation 13 is described in [presentation.md](presentation.md). Operations
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
- A clear "known" bit means unknown. Clients do not turn unknown into
  absent, or the other way round.
- The block stays 64 bytes. A future layout would use a new `version`, and
  a terminal would answer an unknown version with an error, never with a
  reinterpreted block.
