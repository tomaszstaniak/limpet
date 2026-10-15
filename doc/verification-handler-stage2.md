# Verification record: LIMPET: handler, stage 2 (local candidate)

Not published. Three console contracts after Limpet 0.5.0: the Amiga
console's reports, Ctrl-D/E/F, and end of input from the keyboard. They
apply to every Limpet console session (an ordinary Limpet window and the
experimental `LIMPET:` handler alike). `LIMPET:` stays experimental and does
not stand in for `CON:`.

## What the system does (sources)

ABIv11 `deadwood2/AROS` master `e33ca77d`, mainline
`aros-development-team/AROS` (GitHub, 2026-10-15):

- Reports, `rom/devs/console/stdconclass.c`: `C_WINDOW_STATUS_REQUEST`
  (`CSI SPACE q` or `CSI 0 SPACE q`) answers `0x9B "1;1;%d;%d r"`, and
  `C_DEVICE_STATUS_REPORT` (`CSI 6 n`) `0x9B "%d;%dR"`, injected into the
  console's input. ABIv11 passes `cu_YMax, cu_XMax` and `cu_YCP, cu_XCP`,
  which are zero-based; mainline commit `d6ac4e1b27` ("console: improve
  classic application compatibility", 2026-07-21) adds 1 to each, which is
  what AmigaOS answers. Both always use the 8-bit CSI.
- Ctrl-C to F, `rom/filesys/console_handler/support.c` (ABIv11 ~1057):
  cooked mode only, `Signal(breaktask, 1 << (12 + n))`, nothing stored;
  `breaktask` is the last FIND/READ/WRITE sender. Raw mode passes bytes and
  signals nothing.
- End of input, same file ~986-1042 and 1173: Ctrl-\ (or the close gadget,
  which console.device turns into 0x1C) sets `FHFLG_EOF`; text already
  typed is handed to pending reads without a line end; the first pending
  read (or the next read) gets 0 and the flag clears, so later reads wait
  for input again. The Shell ends on that 0 at its prompt.

LIMPET: answers the reports one-based (AmigaOS, mainline) and sends the
break signals to the program reading the window, as Ctrl-C already did.

## Build

`x86_64-aros-gcc` 10.5.0, ABIv11 2026.09 Developer SDK, branch
`handler-stage2` from `main` d8689f6, `LIMPET_VERSION=0.5.1-dev`. Each of
the four code commits builds on its own.

## Host tests

`make test` passes. New: `tests/test_amiga_reports.c` (with
AddressSanitizer and UBSan): bounds with the 8-bit and the 7-bit CSI, with
and without the 0; after a resize; a query split over four writes; the
cursor in the query's own introducer; `0x9B` inside UTF-8 (U+049B) stays a
character; an ordinary VT session neither answers `SPACE q` nor takes
`0x9B` as CSI.

## On AROS

Pool slot v11-2, AROS One 1.3 (ABIv11), QEMU TCG, 2026-10-15, two boots.
`tests/con_contract.c` ran with `keys` on `CON:20/360/640/300/Keys/CLOSE`
and on the same name with `LIMPET:`. Ctrl-\ cannot be sent through QEMU's
keyboard to AROS One (its `backslash` key reaches no character), so
`tests/key_inject.c` (`key-inject 0x0d 0x0008 DELAY`) pressed it through
input.device, as a keyboard does.

| Check | CON: (ABIv11) | LIMPET: |
| --- | --- | --- |
| Window bounds, 8-bit query | `9B 1;1;18;75 r`, unit 19x76: one lower | `9B 1;1;13;75 r`, unit 13x75: one-based |
| Bounds after resizing the window | `1;1;14;64 r`, unit 15x65: one lower | `1;1;10;64 r`, unit 10x64: one-based |
| Cursor after `CSI 3;5H` | `9B 2;4R`: one lower | `9B 3;5R`: one-based |
| Same after the resize | `2;4R` | `3;5R` |
| Replies read a byte at a time | put together | put together |
| Ctrl-C, D, E, F in cooked mode | each signals the reader | each signals the reader |
| Ctrl-D in raw mode | byte 4, no signal | byte 4, no signal |
| Type `xy`, Ctrl-\ | first read `xy`, second read 0 | the same |
| Reading again after that | `z` read normally | the same |
| `Execute` of a script, Ctrl-D during `Wait` | `Shell: ***Break`, the rest skipped | the same |
| Ctrl-\ at the Shell prompt | the Shell ended | the Shell ended, the window closed; nothing left in `Status` |
| Contract probe, all checks | `failures=0` | `failures=0` |

The first LIMPET: run counted Ctrl-D/E/F as failed. That was the probe's
driver: it pressed Return after Ctrl-C, and the empty line it left was read
as the answer to the Ctrl-D prompt, so every later prompt was off by one.
The rerun without that Return passed.

Seen on the way, not changed: in raw mode `CON:` turns a line feed into
CR LF (it sets LF mode, `CSI 20h`), `LIMPET:` does not, so a raw program
writing bare line feeds prints stepped lines in `LIMPET:`.

## Review

An independent review of the six commits found, and these were fixed:

- Ctrl-\ bypassed the wait for a running TAB search in a LIMPET: window;
  the Shell could end while the search child read its directories (Ctrl-C
  had the same window). Both now wait for the search.
- Ctrl-\ jumped ahead of a paste going in; it now keeps its place.
- Ctrl-D/E/F were lost before the first read; the opener now gets them, as
  CON: gives them, and a key with nobody to signal says so.
- A pending end of input survived Ctrl-C; `--command` sessions ignored
  Ctrl-D/E/F and Ctrl-\; `WaitForChar` and the status query ignored a
  pending end; a stray 0x9B could swallow a line end (controls inside a
  sequence now act, ECMA-48); 0x9B inside a sequence did not restart it.

Host tests cover the parser changes; the session changes were built and
checked on AROS with the key probe after the fixes (below).

## Not checked

- Mainline; ABIv11 2026.09; real hardware.
- A program from outside the tree that uses the window-size query (none in
  the ABIv11 tree's `C:` commands or utilities does).
- Ctrl-\ while a command other than the probe reads the console.
