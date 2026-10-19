# Verification record: Limpet 0.6.1 (candidate)

Limpet 0.6.1 fixes two things found while porting Helix: letters of a
keymap in another character set (a Polish ą arrived as ±), and a stray
input line after a full-screen editor quit (`ESC [ 0 SPACE q`, xterm's
cursor-style reset, was answered as the Amiga console's window-size query).
The experimental `LIMPET:` handler archive is built from the same sources.
SDK 0.4.0 is unchanged.

Scope against `v0.6.0`: the CI pin of the reviewed protocol header
(`aac268e`, no program change), the window-size query only with the
8-bit CSI (`c703910`), the keyboard character set (`d10ca35`) and their
documentation.

## Build

| Item | Value |
| --- | --- |
| Sources | `c9cdb485ac69e1d83fbbb1988ce84049105774ac` |
| Compiler | `x86_64-aros-gcc` 10.5.0 |
| SDK | ABIv11 2026.09 Developer SDK |
| Commands | `make abiv11-frontend LIMPET_VERSION=0.6.1`, then `make abiv11-handler` with the same variables, then `tools/release/make-packages.sh 0.6.1 OUT abiv11` |
| Program | 3,418,640 bytes, 1,728 more than 0.6.0 |
| Archives | `Limpet-0.6.1-abiv11.zip`, `Limpet-handler-experimental-0.6.1-abiv11.zip`; hashes in their `SHA256SUMS` |

## Host tests

`make test` passes. Changed: `tests/test_amiga_reports.c` (the 8-bit
`CSI 0 SPACE q` and `CSI SPACE q` still answer; `ESC [ 0 SPACE q`,
`ESC [ SPACE q` and `ESC [ 2 SPACE q` answer nothing and print nothing);
`tests/test_input_echo.c` (ISO-8859-2 bytes of the Polish keymap become
ą ż ł ś ć ę ń ó Ą; Alt, an unmapped byte, a three-byte character and a full
buffer).

## On AROS

Pool slot v11-2, AROS One 1.3 (ABIv11), QEMU TCG, 2026-10-19.

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

Not run again on this candidate: the 0.6.0 checks of Ctrl-D/E/F, Ctrl-\,
Shift+Enter and the handler archive (their code paths did not change; the
handler archive was built, not run). Without `ENV:CHARSET` the old Latin-1
behaviour is kept by design and was not re-checked on AROS.
