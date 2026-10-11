# Verification record: Limpet 0.2.1

Limpet 0.2.1 is Limpet 0.2.0 with HarfBuzz 14.5.0 instead of 11.2.1.
Nothing else in the program changed. The protocol and the SDK are the same
as for 0.2.0, so the SDK has no new release. What was checked, where, and
what was not.

## Build

| Item | Value |
| --- | --- |
| Commit | tag `v0.2.1` |
| Build | `make abiv11-frontend LIMPET_VERSION=0.2.1 LIMPET_DATE=11.10.2026`, `x86_64-aros-gcc` 10.5.0, ABIv11 2026.09 SDK |
| Program | SHA-256 `e3ae6c5d64d1d19339b5dcfbe9515c4a300cd4aca2c7c5cbbd1dab042b217377`, the same bytes from a clean checkout and from the working tree |
| Size | 3,357,536 bytes; 0.2.0 was 3,199,064 (+158,472 bytes, +4.95 %) |
| Archive | `Limpet-0.2.1-abiv11.zip`, SHA-256 `71dee9c638ba7676e8089f75d65511b71df9d066e0436feabd75ee27230c768d`. Against 0.2.0 only `Limpet/Limpet` and `Limpet/README.txt` differ; fonts, themes and licence files are byte-identical |

The same recipe with `LIMPET_VERSION=0.2.0 LIMPET_DATE=07.10.2026` on tag
`v0.2.0` gives the published 0.2.0 program byte for byte, so the size
comparison is between like builds.

## Host tests

`make test-sdk`, `make test`, `make test-font test-font-memory` and
`make test-shaping-corpus` pass. The shaping corpus (110 lines: combining
marks, Hangul, CJK, emoji sequences, flags, keycaps, box drawing,
unassigned and private-use code points) draws the same with 14.5.0 as with
11.2.1: same source font, glyph count, status, size and pixel hash for
every cluster. 400 damaged-font variants ran without a crash; with a font
whose tables are damaged, a cluster may now fall back to another font or
lose a ligature where 11.2.1 still used the damaged table.

## On AROS

AROS One 1.3 (ABIv11, x86_64), QEMU TCG, 2026-10-11, from exactly the
archive above, unpacked to `RAM:`:

| Check | Result |
| --- | --- |
| `Version FULL` | `Limpet 0.2.1 (10/11/26)` |
| Start with a Shell | prompt in the Limpet window |
| Unicode file shown with `Type` | Polish, CJK, Hangul, monochrome emoji, combining marks and box drawing drawn |
| Selection, RAmiga+C, RAmiga+V into a command | the pasted text written to a file equals the source line byte for byte (`COPYME-ŁÓDŹ-漢字`, UTF-8) |
| `EndCLI`, close gadget | window closed; `Status` shows no Limpet or session process; the system Shell keeps running commands |

Earlier, on AROS ABIv11 2026.09: the shaping corpus shown in a Limpet
window looked the same with both HarfBuzz versions, and long sessions of a
full-screen client (selection and copy while text streamed in, then a
clean exit) passed with both. Those long sessions ran with a corrected
`e1000.device` in the guest, because the stock network driver of these
systems damages memory under heavy transmit. That is a driver fault,
unrelated to Limpet, and this release does not change it.

## Speed

Not settled. In the long sessions above, screen redraws took about 1.8x
longer with 14.5.0 (medians 10.0-15.8 ms against 7.8 ms), with the
emulator's window state not controlled. A shorter A/B measurement without
network, alternating the two builds in one boot, did not show that
difference, but the guest's speed changed about 3x during it, so it does
not rule out a smaller one. Shaping alone timed the same within the
spread.

## Not checked

Mainline AROS (no build of this version), real hardware, the mounted
console handler with 0.2.1, and the SDK examples again (the SDK did not
change).
