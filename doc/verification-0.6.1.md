# Verification record: Limpet 0.6.1

Limpet 0.6.1 fixes what porting Helix found: letters of a keymap in another
character set (a Polish ą arrived as ±); a stray input line after a
full-screen editor quit (`ESC [ 0 SPACE q`, xterm's cursor-style reset, was
answered as the Amiga console's window-size query); a program waiting for
keys did not notice its window being closed (`WaitForChar` false at end of
input, writes refused with `ERROR_BREAK`); and Vera Mono's w at 12 px. New:
margin colours on the alternate screen and `COLORTERM=truecolor` for
sessions. The experimental `LIMPET:` handler archive is built from the same
sources. SDK 0.4.0 is unchanged.

Scope against `v0.6.0`: the CI pin of the reviewed protocol header
(`aac268e`, no program change), the window-size query only with the
8-bit CSI (`c703910`), the keyboard character set (`d10ca35`), the 12 px w
(`29fe585`), margins (`81fb7a1`), COLORTERM (`2ad7b6f`), WaitForChar at end of
input (`3c3c6c5`), writes to a closed window (`604d04e`), Latin-1 for an
unknown character set (`e6f354a`, after review), documentation.

A first candidate (sources `c9cdb48`) was tested on 2026-10-19 night with the
first two fixes; its archives are kept in `dist/0.6.1-candidate1/` and were
not released. Candidate 3 (`e46ed2c`, `dist/0.6.1-candidate3/`) passed the
checks below except the review's character-set point; the released archive
was then rebuilt and checked again (last table).

## Build

| Item | Value |
| --- | --- |
| Sources | `247999396645b03df40e7918d8943bdedb9e37f7` |
| Compiler | `x86_64-aros-gcc` 10.5.0 |
| SDK | ABIv11 2026.09 Developer SDK |
| Commands | `make abiv11-frontend LIMPET_VERSION=0.6.1`, then `make abiv11-handler` with the same variables, then `tools/release/make-packages.sh 0.6.1 OUT abiv11` |
| Program | 3,421,704 bytes, 4,792 more than 0.6.0 |
| Archives | `Limpet-0.6.1-abiv11.zip`, `Limpet-handler-experimental-0.6.1-abiv11.zip`; hashes in their `SHA256SUMS` |

## Host tests

`make test` passes. Changed: `tests/test_amiga_reports.c` (the 8-bit
`CSI 0 SPACE q` and `CSI SPACE q` still answer; `ESC [ 0 SPACE q`,
`ESC [ SPACE q` and `ESC [ 2 SPACE q` answer nothing and print nothing);
`tests/test_input_echo.c` (ISO-8859-2 bytes of the Polish keymap become
ą ż ł ś ć ę ń ó Ą; Alt, an unmapped byte, a three-byte character and a full
buffer); `tests/test_margin_fill.c` (new: theme background on the normal
screen, edge cells' backgrounds on the alternate screen per row and column,
corners, inverse, selection ignored, restore, a window smaller than the
grid); `tests/test_session_queue.c` (a waiter at end of input is ready);
`tests/test_aros_endpoint.c` (writes to a stopped session fail with
`ERROR_DEVICE_NOT_MOUNTED`; built, run only as part of the AROS test set).
`make test-font` with Vera Mono and Open Sans from the AROS tree adds
`tests/test_cluster_letters.c` (new: the x-height letters keep the baseline
of x at 10-18 px; before the fix it failed at 10 px), and the shaping corpus
at 13 px is unchanged.

## On AROS

AROS One 1.3 (ABIv11), QEMU TCG, 2026-10-19: candidate 1 on pool slot v11-2
(night), the released candidate on v11-1 (late morning).

Before the fixes, with Limpet 0.6.0:

| Check | Result |
| --- | --- |
| Polish keymap (`SetKeyboard pc105_pl`), RAlt+a, z, x, l, o, s, c, e, n | a raw-mode reader got ± ¿ ¼ ³ ó ¶ æ ê ñ: Latin-1 for ISO-8859-2 bytes |
| Helix quits (it sends `ESC [ 0 SPACE q` while still in raw mode), then `Echo` | the Shell ran a stray line first: "bject not found" |

With the candidate archive `Limpet-0.6.1-abiv11.zip` unpacked to RAM:
(and Helix 25.07.1 for AROS, port release 1):

| Check | Result |
| --- | --- |
| `Version Limpet FULL` | Limpet 0.6.1 |
| `SetEnv CHARSET ISO-8859-2`, Polish keymap, typing ą ż ł ś ć ę ń Ż in Helix | the letters as typed; the saved file holds their UTF-8 bytes (c4 85, c5 bc, c5 82, c5 9b, c4 87, c4 99, c5 84, c5 bb) |
| Helix quits twice, then `Echo` each time | rc 0, no stray input |
| Window resized while Helix runs | Helix redraws at the new size |

With the released archive (sources above, `Limpet-0.6.1-abiv11.zip` as in
the release's `SHA256SUMS`), unpacked to RAM: with Helix preview 1:

| Check | Result |
| --- | --- |
| `Version Limpet FULL` | Limpet 0.6.1 |
| `Get COLORTERM` in the window's Shell, and in a `Run` job | `truecolor` |
| `GetEnv COLORTERM` (global) | object not found |
| Helix without any `SetEnv` | its 24-bit default theme; the margins in its background colour |
| Helix quits, then the window again | margins in the theme background |
| `font_size=12`, `Type` of "new await window w W ww" | w at the letters' baseline, no gap (before: raised, smaller) |
| Second window, Helix running, close gadget | window closes, Helix and the window's Limpet end (`Status`), host CPU 2.5 % (before: window stayed on "Closing: waiting for process", 97-99 %) |

Released archive, final short check (pool v11-1, 12:50): `Version`
0.6.1; Polish letters typed in Helix saved as UTF-8 to a file and `Type`d
back; second window closed while Helix ran: window, Helix and its Limpet
ended, host CPU 2.6 %.

Not run again on this candidate: the 0.6.0 checks of Ctrl-D/E/F, Ctrl-\,
Shift+Enter and the handler archive (their code paths did not change; the
handler archive was built, not run). Without `ENV:CHARSET` the old Latin-1
behaviour is kept by design and was not re-checked on AROS.
