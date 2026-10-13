# Verification record: Limpet 0.4.1

Limpet 0.4.1 changes two things against 0.4.0: the Shell session takes a
client's pending read back (`XTY_ACTION_CANCEL`, read only, by the
packet's identity), and closing the window under drops no longer relies
on a uniprocessor exec. Limpet SDK stays 0.3.0: nothing under `sdk/`
changed since `sdk-v0.3.0`, and the header already defined the cancel.

## Build

| Item | Value |
| --- | --- |
| Sources | tag `v0.4.1` |
| Build | `make abiv11-frontend LIMPET_VERSION=0.4.1 LIMPET_DATE=13.10.2026`, `x86_64-aros-gcc` 10.5.0, ABIv11 2026.09 Developer SDK, HarfBuzz object built in the same tree |
| Program | 3,386,296 bytes, SHA-256 `1575f4e220006de48c01ba3391870432ceab57093211243c34fa0e075663ea38`; 0.4.0 was 3,385,384 (+912 bytes) |

## Host tests

`make test` passes. The review of both changes is recorded in the
commits (`27aaafe`, `4b75a7f`, `de74d91`).

## On AROS

Pool slot v11-2, AROS One 1.3 (ABIv11), QEMU TCG, 2026-10-13, with the
program above (`Version`: Limpet 0.4.1).

| Check | Result |
| --- | --- |
| `read-cancel` (`tests/read_cancel.c`): a read sent, then cancelled | the read answered first, -1 with `ERROR_BREAK` (304); then the cancel, `DOSTRUE`; the next key reached a plain `Read` |
| `drop-close` (`tests/drop_close.c`) | every check PASS: a drop during the close answered without the task; the close's answering process gone after each close; three senders: 87 taken, 11 answered in the close, 98 replies, each sender ended |
| hardcode (`second-agent-gate` `9ccb0f1`) in the window: a turn, `/quit`, `Echo clean` | clean: hardcode took its read back |
| hardcode, a key typed just before the cancel (test build) | the read took the key, hardcode ended, the Shell got nothing |
| The window closed while hardcode streamed a reply | hardcode and Limpet gone from `Status` |

The same `read-cancel` in CON: is refused (`ERROR_ACTION_NOT_KNOWN`) with
the read still out; Limpet 0.4.0 refuses it too.

## Not checked

- An SMP exec. The new close is meant to be safe there (the port is freed
  only after the answering process has exited, by `ChildStatus`), but
  that rests on reading the code: no SMP system was run.
- Mainline, ABIv11 2026.09 at runtime, real hardware.
- A drop from a program other than Wanderer or the test's own senders.
