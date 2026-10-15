# Verification record: LIMPET: handler, stage 3 (local candidate)

Not published. Two console contracts after stage 2: several clients reading
and waiting at once, and line feed mode. They apply to every Limpet
console session; `LIMPET:` stays experimental and does not stand in for
`CON:`.

## What the system does (sources, ABIv11 `e33ca77d`)

- Reads, `rom/filesys/console_handler/support.c` `con_read`,
  `HandlePendingReads` (~1114-1180): reads that find no complete line wait
  in a list; a finished line goes to them in order, each taking up to its
  length, until the text runs out.
- `WaitForChar`, `con_handler.c` (~879-904, 708-723): answered TRUE at
  once while any input is held (a partly typed line counts), FALSE at once
  for a zero timeout; otherwise the packet goes into a single slot,
  `waitingdp`, with the handler's one timer request. A second waiter
  overwrites the slot (the first is never answered) and sends the timer
  request again while it is still out.
- Line feed mode, `rom/devs/console/consoleclass.c:127`: "RKM: Default
  mode is SET", so a line feed also returns the carriage; `CSI 20 h` sets
  it and `CSI 20 l` clears it (`C_SET_LF_MODE`, `C_RESET_LF_MODE`); the
  console handler also sends `CSI 20 h` when it opens a window.

## Build

`x86_64-aros-gcc` 10.5.0, ABIv11 2026.09 Developer SDK, branch
`handler-stage3` from `main` after the stage 2 merge,
`LIMPET_VERSION=0.5.1-dev`. Each code commit builds on its own.

## Host tests

`make test` passes. `tests/test_amiga_reports.c` adds line feed mode: on
in a console session, `CSI 20 l` and `CSI 20 h` with both introducers, a
reset screen following the session again, VT off by default and on with
`CSI 20 h`. The session's queues are AROS-only code;
`tests/test_aros_session.c` was ported to the new fields and
syntax-checked, not run.

## On AROS

Pool slot v11-1, AROS One 1.3 (ABIv11), QEMU TCG, 2026-10-15. The probe's
new `shared` mode: a child reads before this process (both lines must go in
order), a child waits for a key while this process reads, and two waiters
time out with no key, each child with a watchdog so a lost packet shows as
such.

| Check | CON: (ABIv11) | LIMPET: |
| --- | --- | --- |
| Line feed from column 10, default | column 0 (zero-based report): returns | column 1: returns |
| After `CSI 20 l` | column 9: stays | column 10: stays |
| After `CSI 20 h` | returns | returns |
| Two readers, `one` then `two` | child `one`, this process `two` | the same |
| A waiter beside a read, key typed | guest hung (QEMU 95 % CPU, no input), power-cycled | waiter TRUE, read `w` |
| Two waiters, no key | not reached | both FALSE after their timeouts |

The first LIMPET: run failed the waiter beside a read: the typed key stayed
in Limpet's line editor, which the waiter did not count, while CON: counts
a partly typed line; and the driver's key came at the edge of the 3 s
timeout. The editor's text now counts, and the probe waits 10 s; the rerun
passed.

The keys probe of stage 2 ran again on this build: `failures=0`. In a
`LIMPET:` Shell, `List`, `Type`, `Execute` and `Ask` printed without stepped
lines.

The CON: hang was seen once and not repeated; from the source it fits the
single waiting slot and its timer request sent again while out. It is
recorded as an observation about the system console, not investigated
further.

## Not checked

- More than two readers or waiters at once; the queue limits (16).
- A real program that relies on line feed moving down only (none of the
  Limpet clients checked does: hardcode and the SDK examples write CR LF).
- Mainline; ABIv11 2026.09; real hardware.
