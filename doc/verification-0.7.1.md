# Verification record: Limpet 0.7.1

0.7.1 changes one thing in the program: the result of Ctrl+click on a
`file:///` link. It also renames the release targets and archives, builds
all three systems through `make target`, and adds guest test tools. This
record covers the build, the runtime runs of the release candidate on all
three targets, the link tests that 0.7.0 left open, and a review of the
platform boundaries against current mainline.

## What changed in the program

- `src/aros_hyperlink.c`: a `file:///` link is checked with `Lock()` before
  `OpenWorkbenchObjectA()`. workbench.library queues a start and reports a
  start that fails to nobody (`workbench/libs/workbench/handler.c`, "report
  error somehow?", still on mainline master `42674abe`), so without the
  check a missing file gave no answer at all.
- `src/aros_xterm.c`: the window title reports file links as "File handed
  to Workbench", "File not found; Ctrl+Shift-click copies", "Workbench
  could not open the file" and "workbench.library unavailable". Before,
  file links reported OpenURL's messages. Web links are unchanged.

The worker of the `LIMPET:` handler and LimpetPTY are built from the same
sources and get the same change. LimpetPTY was compiled for mainline, not
run (it is a separate frontend, not part of this release).

## Build

| Target | Compiler | SDK |
| --- | --- | --- |
| `aros-x86_64-abiv11-pc` | ABIv11 `x86_64-aros-gcc` 10.5.0 | ABIv11 2026.09 Developer SDK (`SDK-202609`) |
| `aros-x86_64-abiv1-pc` | mainline `x86_64-aros-gcc` 10.5.0 | mainline Developer SDK of `74a2d80b` (2026-09-04) |
| `aros-aarch64-abiv1-raspi` | AROS Clang 20.1.0 wrapper | Developer SDK of the raspi-aarch64 system of 2026-08-22, libc++ headers only |

    make target TARGET=<target> LIMPET_VERSION=0.7.1 LIMPET_DATE=27.10.2026 <T_* variables>
    tools/release/make-packages.sh 0.7.1 OUT aros-x86_64-abiv11-pc aros-x86_64-abiv1-pc aros-aarch64-abiv1-raspi

Each target builds in an empty `build/<target>/`, HarfBuzz included; no
object is shared between targets. Source commit of the candidate:
`dist/0.7.1-candidate1/SOURCE-COMMIT`; binary hashes:
`dist/0.7.1-candidate1/binaries.sha256`.

**The new build reproduces 0.7.0.** Built from the 0.7.0 sources
(`LIMPET_VERSION=0.7.0 LIMPET_DATE=24.10.2026`), the three targets gave
the released `Limpet`, `limpet-console`, `limpet-handler` and
`limpet-handler-remove` of all three systems byte for byte (ABIv11 `Limpet`
`f24cb86f…`, worker `de2f2dfe…`; mainline `5a5fcffd…`; Pi `16955fdc…`).
The ABIv11 worker needed its old link order for that
(`T_WORKER_ALL_SRC` in `mk/target-aros-x86_64-abiv11-pc.mk`). The packages
made by the new `make-packages.sh` from those binaries matched the
released 0.7.0 contents file for file, except the corrected ReadMe files.

Archives of the candidate:

| Archive | Bytes | SHA-256 |
| --- | --- | --- |
| `Limpet-0.7.1-aros-x86_64-abiv11-pc.zip` | 5,305,602 | `652e3fd8df4640fbe94f3ea72644c315af3fa49cdc9c6de908106585964baf3c` |
| `Limpet-handler-experimental-0.7.1-aros-x86_64-abiv11-pc.zip` | 1,050,320 | `463ba65a1b48c24b6aaa8049401c5ea5967ea76cb11ce1741492cd4f779d5301` |
| `Limpet-0.7.1-aros-x86_64-abiv1-pc.zip` | 5,219,873 | `77df5c255424aae214ac6b6fd22b5fb160847cd07f8a931ff44d52d7f2af97ae` |
| `Limpet-handler-experimental-0.7.1-aros-x86_64-abiv1-pc.zip` | 968,083 | `a60989e02c49dd786cd7537f37d60f97a6924dddee1868eb47eb4ee92bf1ad3b` |
| `Limpet-0.7.1-aros-aarch64-abiv1-raspi.zip` | 5,150,299 | `1ad79d53cbdd71ffa93689a8cf177b2b054358104fc0a51b1d70bd24707e1958` |
| `Limpet-handler-experimental-0.7.1-aros-aarch64-abiv1-raspi.zip` | 897,996 | `5be2bcbe01003e4c18c30b0f0f309fc1fdaf715e995e7600f21120bd916c8fc4` |

Host: `make test` passes (43 Python tests, fixtures, replay corpus).

## How links were tested

A link test has to separate two things: that Limpet hands over the exact
target, and whether a browser then shows the page. The second is outside
Limpet and was not tested. For the first:

- `tests/link_logger.c` stands in for the browser and for a default tool.
  It appends what it was started with to a file: its arguments when OpenURL
  runs it, the full name of its Workbench argument when Workbench runs it.
- `tests/openurl_set_browser.c` makes OpenURL run the logger for every web
  link (`RAM:lt/link-logger "%u"`), in `ENV:` only.
- A project icon made by `link-logger --icon` gives `doc one.txt` the
  logger as its default tool; `notool.txt` gets a default tool that does not
  exist; `plain.txt` has no icon (the system's default tool for text).
- The fixture prints five OSC 8 links: `file:///RAM:lt/doc%20one.txt`,
  `https://example.com/p/a?b=1&c=%C5%BC#frag`, a missing file, the file
  with the missing tool, and the plain file.

On x86_64 the gesture was a real Ctrl+click through QEMU's tablet. On the
Pi model the pointer cannot be aimed, so `tests/win_inject.c` makes the
click inside the guest: it moves the pointer with `IECLASS_NEWPOINTERPOS`
and presses Ctrl as a key, then the button, through input.device. That is
the same path a mouse takes from input.device on; QEMU's pointer is the part
left out. Intuition takes a button's qualifiers from the keyboard state, so
an event qualifier alone gave a plain click ("Selected"); with the Ctrl key
pressed it works. On mainline x86_64 the same tool clicked where it was
told and the Ctrl+click was recognised, which checks the tool against the
real gesture. Window size was changed with `win-inject size`
(`ChangeWindowBox`): the window gets the same new size as from a drag of
its size gadget, but the drag itself is not exercised on the Pi.

## Runtime

All runs from the candidate archives, unpacked to `RAM:` (x86_64 with the
pool machine's UnZip; on the Pi the unpacked drawer was copied to the
stopped machine's SD card), QEMU TCG, uniprocessor exec on every system.

| | ABIv11 | mainline x86_64 | Raspberry Pi |
| --- | --- | --- | --- |
| Machine | pool `v11-1`, AROS One 1.3 | pool `v1-1`, mainline `74a2d80b` (2026-09-04), exec 51.9 | `rpi-1`, QEMU `raspi3b`, Pi system of 2026-08-22, exec 51.8 |
| `Version` of `Limpet` and `limpet-console` | 0.7.1 (10/27/26) | 0.7.1 | 0.7.1 |
| `file:///` with an icon | "File handed to Workbench"; logger got `RAM Disk:lt/doc one.txt` | same | same |
| HTTPS | "Link handed to OpenURL"; logger got the URL byte for byte | same | same (test1 build) |
| Missing file | "File not found; Ctrl+Shift-click copies" | same | same |
| Default tool missing | "Workbench could not open the file" | same | same (test1 build) |
| Plain file, no icon | opened in JanoEditor | opened in the text viewer | opened in the text viewer (test1 build) |
| One click, one start | checked: L1, L2, L5 and a click in the handler window each added one line (L5 none) | three clicks, three lines | two clicks, one line (the other was the missing file) |
| Window resize | (unchanged code; 0.7.0 record) | `win-inject` 684x478 to 520x330; geometry packet: all checks pass | 640x478 to 440x300; grid 74x24 to 49x13, 392x221 px, public and private agree |
| `LIMPET:` handler | install, `NewShell WINDOW LIMPET:`, worker 0.7.1, Ctrl-C ends `Wait 30`, links in the handler window, `EndShell`, Remove-Handler, `Status` with no Limpet process but the frontend | same | same, without clicking links in the handler window |

Development runs before the candidate (`0.7.1-test1`, same program code):
on mainline, the HTTPS link with OpenURL pointed at a program that does not
exist, and with `openurl.library` removed. The first reported "Link handed
to OpenURL": OpenURL starts the browser asynchronously and returns success,
so Limpet cannot know that nothing opened. The second reported
"openurl.library unavailable; Ctrl+Shift-click copies". The library was
renamed for that click and renamed back (same size and date). Ctrl+Shift+
click copied the exact URL on mainline and on the Pi (pasted with Amiga+V).
On the Pi the 0.7.1 code also ran the five links, the resize and the copy
(`test1`, runs `pi1`).

One count on ABIv11 first showed two log lines too many. The test driver
had sent the four clicks twice (a shell error stopped one command of the
line, not the loop). Counted click by click afterwards: one start each.

Not tested: a browser rendering the page; physical hardware; SMP; the
resize drag gesture on the Pi; `TIOCGWINSZ` on a system newer than the
pool's mainline base (see below); Macaros.

## Platform review

Current mainline master (`42674abe`, 2026-10-26) was compared with the
three SDKs and with Limpet's code at each system boundary. Summary; the
full notes are kept with the local evidence.

- **Window size.** Limpet answers both `ACTION_CONSOLE_GETGEOMETRY`
  (open AROS PR #1763) and the old `ACTION_DISK_INFO` reply, which ABIv11,
  the Pi system of 2026-08-22 and mainline `74a2d80b` read. Mainline's C
  library since `ca3b61dff6` (PR #1170, 2026-09-09) answers `TIOCGWINSZ`
  with ENOTTY for every console until it uses the geometry packet, so on
  newer mainline systems `TIOCGWINSZ` in Limpet fails although Limpet
  answers. Documented in the capability reference; nothing to change in
  Limpet.
- **Ctrl-C.** On mainline Limpet uses task.resource's task notification
  (`AddTaskNotifyHook`, task.resource 1.4, `83b288ac`, 2026-08-29), found at
  run time by the resource's version. The Pi SDK has `resources/task.h`
  without the notification calls and ABIv11 has neither, so both are built
  with the listed-task method. A run-time fallback to that method on older
  mainline was considered and left out: the code deliberately has no
  fallback to a task pointer there, because the method is not safe under an
  SMP exec. Mainline systems older than 2026-08-29 therefore get no Ctrl-C;
  the mainline ReadMe now says so.
- **Pending reads and cancel.** `AbortPkt` is still a no-op on master and
  there is no public read cancel; Limpet's own cancel packet with
  `ERROR_ACTION_NOT_KNOWN` from other consoles remains the way to detect
  it.
- **Handler and `fh_Type`.** Mainline and the Pi keep the handler's
  `fh_Type`; ABIv11 still overwrites it, so its worker serves the process
  port (`XTERM_PROCESS_PORT_CLIENTS`, chosen at build time).
- **Clipboard.** No UTF-8 handling in ConClip on master; Limpet keeps
  reading `CHRS` as Latin-1. Using the system charset is backlog.
- **SMP.** None of the tested systems runs an SMP exec. Limpet no longer
  uses `PA_SOFTINT` or `KrnGetCPUCount`.
- **Libraries.** FreeType is linked to the system's freetype2.library on
  all three targets; a newer mainline SDK would bring no call Limpet uses.

No reusable system fix came out of this round, so no mainline PR was
prepared. Open upstream items that bear on Limpet: #1763 (geometry), #1874
(CON: timed waits; the CON: control runs should be repeated after it is
merged).

Evidence (local, not in git): `docs/evidence/limpet-071-20261027/`
(`links-x86_64/`, `pi/`, `candidate1-abiv11/`, `candidate1-abiv1-x86_64/`,
`candidate1-pi/`: payload hashes, collected results, screenshots).
