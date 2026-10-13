# Verification record: Limpet 0.4.0

Limpet 0.4.0 adds one feature: icons dropped from Workbench put their paths
on the Shell's command line. The protocol and the SDK are unchanged
(SDK 0.3.0). Published 2026-10-13 as tag `v0.4.0`. Design notes:
`docs/file-drop.md` in a working checkout (not published).

## Build

| Item | Value |
| --- | --- |
| Sources | `main` at `6540e27`: the candidate (`2ad5944`, four commits on `59467e0`: arguments, AppWindow, guest tools, docs) and the close fix with its review (`8b370de`, `3c3d105`, `6540e27`) |
| Build | `make abiv11-frontend LIMPET_VERSION=0.4.0 LIMPET_DATE=13.10.2026`, `x86_64-aros-gcc` 10.5.0, ABIv11 2026.09 Developer SDK |
| Program | 3,385,384 bytes, SHA-256 `2904e99709a07071f8e23db822ebf682b8cd40b2038ebc8cd3b73a49e36959bf`; 0.3.0 was 3,375,824 (+9,560 bytes, +0.28 %) |
| Archive | `Limpet-0.4.0-abiv11.zip`, 3,653,872 bytes, SHA-256 `49e606fd8609588524c6e69db5537707584965fa4dd08da9bb545609c435c64f`, from `tools/release/make-packages.sh 0.4.0 … abiv11`. The candidate of 2026-10-12 (`388b78ab…`) was not published |

## Host tests

`make test` passes. New: `tests/test_file_drop.c`. It covers:

- quoting of each character the Shell, ReadArgs or the C start-up reads
  specially, and plain paths left as they are;
- no POSIX quoting;
- controls, C1, Latin-1 and empty names refused;
- spacing at the start, middle and end of a line and between spaces;
- a selection kept as text, and order kept;
- all or nothing, and the limits of 1023 bytes and 64 objects;
- the Ctrl-R refusal;
- the Shell's quote miscount (below).

The second commit (`33245f9`) builds for ABIv11 on its own, and the first
passes its test on its own.

## On AROS

Pool slot v11-1, AROS One 1.3 (ABIv11), QEMU TCG. Icons were dragged with
QMP pointer events in small steps. The guest tools come from
`make abiv11-drop-tools`:

- `drop-fixture` makes a drawer of awkward names with default icons;
- `drop-args` prints `argv` and ReadArgs `FILES/M` for each argument, and
  whether the path locks.

The development build had the same sources as `2ad5944`, with
`LIMPET_VERSION=0.4.0-dev`.

| Check | Result |
| --- | --- |
| One file, run | `"RAM Disk:dt/*$HOME.txt"` on the line; argv and ReadArgs `RAM Disk:dt/$HOME.txt`, exists, same. `$HOME` was set in that Shell |
| Drop in the middle of a typed line | path before `X`, spaces right; two arguments |
| A drawer with a space, and the RAM Disk volume icon | `"RAM Disk:dt/drawer with space"`, `"RAM Disk:"`; both exist |
| Five names at once: `$HOME`, backticks, `par(en)#?`, `quote"d`, `st*ar` | five arguments, each exact in argv and ReadArgs, in Wanderer's order |
| A UTF-8 name (`zżółw.txt`) and a name with a space | exact, exists |
| An icon without its file | refused: "Drop: ghost.txt is not there" |
| A Latin-1 name (`café.txt`) | refused: "is not UTF-8 text or has a control" |
| During Ctrl-R | refused, the search line unchanged |
| During `Wait 12` | refused; the prompt came back clean |
| hardcode's full screen (raw mode) | refused: "the program here reads keys itself"; hardcode received nothing |
| The About window open | the drop answered at once, Wanderer kept working (an icon selected right after), "close the About window first" |
| After a resize | typing, a drop and the command work |
| Ctrl-R and paste after drops | the search works; RAmiga+C / RAmiga+V pasted the selection |
| Window closed | Limpet gone from `Status`; Wanderer running |
| Candidate archive | unpacked from the zip, `Version` 0.4.0; two files dropped and run (`plain.txt`, `st**ar`); About shows 0.4.0 and the drop line; closed, gone from `Status` |

The Shell's quote miscount was found here. An early build put `quote"d`
and `semi;colon` on one line. `argv` ended at `RAM Disk:dt/semi`, and
ReadArgs failed with error 120. The ABIv11 Shell's `readLine`
(`workbench/c/shell/readLine.c`) flips its quote state at every `"`,
including the escaped `*"`, so the quoted `;` was taken for a comment. The
candidate refuses such a drop. Each name works alone, and so does the `;`
name before the `"` name.

Screens are kept locally in `docs/evidence/file-drop-20261012/` (not
published).

## The close under drops (after review)

A review of the candidate found that a drop arriving between Limpet's last
reply and `RemoveAppWindow` left both waiting: Wanderer's
`SendAppWindowMessage` holds the Workbench semaphore shared until its
reply, and `RemoveAppWindow` takes it exclusively. Since the fix the
AppWindow's port is a software interrupt's (`PA_SOFTINT`, set before
`AddAppWindowA`). It passes drops to the task's inbox, or replies to them
at once once closing has begun. The ports and the interrupt are freed only
after `RemoveAppWindow`, when every sender has had its reply.

`make abiv11-drop-tools` builds `drop-close`, and `make
abiv11-drop-close-old` builds the same check against the candidate's code.
It opens its own window as an AppWindow and sends drops from processes of
its own through `SendAppWindowMessage`, so the order of events is its own:

| Check | Fixed code | Candidate (07268aa) |
| --- | --- | --- |
| 1 A drop reaches the task with its full path; the sender has its reply | PASS | PASS |
| 2 A drop sent after closing began, before `RemoveAppWindow`, is answered without the task; the close ends; a drop after it finds no window | PASS | FAIL: the drop waits for the task (`RemoveAppWindow` would wait for good) |
| 3 Three senders, a tick apart each, while the window closes: every drop with a reply was taken or answered | PASS (93 taken, 18 answered in the close, 111 replies) | not run |

Pool slot v11-1, AROS One 1.3, 2026-10-13. The figures are from the
release code (`6540e27`, built with the 2026.09 SDK); a run on `8b370de`
before the review had the same results (78 taken, 21 answered, 99
replies). In the same boot, with the program from the release archive
(`Version`: Limpet 0.4.0): an icon dropped from Wanderer after `List `
gave `List "RAM Disk:dc.script"`, which listed the file; hardcode's turn
and `/quit` left the Shell clean; and the window closed while hardcode
streamed a reply, with neither left in `Status`.

Two runs before the counted one were faults of the check itself: the
senders called workbench.library through the base that the close sets to
NULL, and three senders that never paused starved `RemoveAppWindow`, as
exec gives a shared request the semaphore at once while it is held
shared, even with an exclusive request waiting. The senders now have their
own library base and pause a tick between drops.

## apkg

The release asset (`Limpet-0.4.0-abiv11.zip` from tag `v0.4.0`, the bytes
above) is in the apkg catalogue since arospkg-index PR #16 (2026-10-13).
Before the merge, on pool slot v11-1 (AROS One 1.3) with apkg 0.5.0 and the
branch's catalogue: 0.3.0 installed from the public catalogue, `upgrade
limpet` to 0.4.0 (rc 0; 2 files replaced, 13 unchanged), `Version` 0.4.0,
`verify` 15 as installed; the upgraded program started and took a drop of
the RAM Disk icon (`List "RAM Disk:"` ran); `remove` left nothing.

## Not checked

- Mainline (the PTY build and `LimpetPTY` do not have the feature, and the
  ABIv11 frontend is the only build that was run).
- ABIv11 2026.09 at runtime, real hardware.
- A drop from another program that sends AppMessages (Directory Opus 4 is
  on AROS One; only Wanderer was used).
- Memory over many drops.
- A drop at the very moment of a real window close from Wanderer.
  `drop-close` (above) sets that order with its own senders instead.
- An SMP exec. The close relies on a uniprocessor exec, which AROS One and
  the mainline SDK in use here have (its `aros/config.h` leaves
  `__AROSEXEC_SMP__` undefined). `aros_drop.c` refuses to compile where
  that macro is defined, but neither SDK exports it, so this binary run on
  an SMP exec would not notice. `KrnGetCPUCount` is no test for it: on PC
  it counts the APICs found, whether exec uses them or not.
